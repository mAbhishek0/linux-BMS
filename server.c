#include "common.h"

// Global array for session management (index = user_id)
// 0 = logged out, 1 = logged in
int active_sessions[5000];
// Mutex to protect the active_sessions array
pthread_mutex_t session_lock = PTHREAD_MUTEX_INITIALIZER;

// --- New: In-process concurrency control ---
#define MAX_ID 5005
static pthread_mutex_t account_mutexes[MAX_ID];     // one mutex per account/user id
static pthread_mutex_t txlog_mutex = PTHREAD_MUTEX_INITIALIZER;

// Canonical two-account locking to avoid deadlocks in A<->B transfers
static inline void lock_account_pair(int a, int b) {
    int lo = (a < b) ? a : b;
    int hi = (a < b) ? b : a;
    pthread_mutex_lock(&account_mutexes[lo]);
    if (hi != lo) pthread_mutex_lock(&account_mutexes[hi]);
}
static inline void unlock_account_pair(int a, int b) {
    int lo = (a < b) ? a : b;
    int hi = (a < b) ? b : a;
    if (hi != lo) pthread_mutex_unlock(&account_mutexes[hi]);
    pthread_mutex_unlock(&account_mutexes[lo]);
}
static inline void lock_account_one(int id)   { pthread_mutex_lock(&account_mutexes[id]); }
static inline void unlock_account_one(int id) { pthread_mutex_unlock(&account_mutexes[id]); }


// --- Function Prototypes ---
void* handle_client_connection(void* client_socket);
void handle_login(int sock, Request* req, Response* res);
void handle_change_password(int sock, Request* req, Response* res); 
void handle_customer_operations(int sock, Request* req, Response* res);
void handle_employee_operations(int sock, Request* req, Response* res);
void handle_manager_operations(int sock, Request* req, Response* res);
void handle_admin_operations(int sock, Request* req, Response* res);
void log_transaction(int acc_id, const char* type, double amount, double new_balance);

// --- Locking Helpers ---
void set_record_lock(int fd, int record_id, int type, size_t struct_size) {
    struct flock lock;
    lock.l_type = type; lock.l_whence = SEEK_SET;
    lock.l_start = (off_t)record_id * (off_t)struct_size;
    lock.l_len = (off_t)struct_size; lock.l_pid = getpid();
    if (fcntl(fd, F_SETLKW, &lock) == -1) { perror("fcntl set lock"); }
}
void unlock_record(int fd, int record_id, size_t struct_size) {
    struct flock lock;
    lock.l_type = F_UNLCK; lock.l_whence = SEEK_SET;
    lock.l_start = (off_t)record_id * (off_t)struct_size;
    lock.l_len = (off_t)struct_size; lock.l_pid = getpid();
    if (fcntl(fd, F_SETLKW, &lock) == -1) { perror("fcntl unlock"); }
}
void set_file_lock(int fd, int type) {
    struct flock lock;
    lock.l_type = type; lock.l_whence = SEEK_SET;
    lock.l_start = 0; lock.l_len = 0; // Lock entire file
    if (fcntl(fd, F_SETLKW, &lock) == -1) { perror("fcntl file lock"); }
}
void unlock_file(int fd) {
    struct flock lock;
    lock.l_type = F_UNLCK; lock.l_whence = SEEK_SET;
    lock.l_start = 0; lock.l_len = 0;
    if (fcntl(fd, F_SETLKW, &lock) == -1) { perror("fcntl file unlock"); }
}

// --- Transaction Logger (updated: serialized by txlog_mutex) ---
void log_transaction(int acc_id, const char* type, double amount, double new_balance) {
    pthread_mutex_lock(&txlog_mutex); // NEW
    int fd = open(TRANSACTION_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) { perror("open TRANSACTION_FILE"); pthread_mutex_unlock(&txlog_mutex); return; }
    
    set_file_lock(fd, F_WRLCK); // existing cross-process safety

    off_t offset = lseek(fd, 0, SEEK_END);
    int trans_id = (int)(offset / (off_t)sizeof(Transaction)) + 1;

    Transaction t = {trans_id, acc_id, time(NULL), "", amount, new_balance};
    strncpy(t.type, type, 19);
    t.type[19] = '\0';
    if (write(fd, &t, sizeof(Transaction)) != (ssize_t)sizeof(Transaction)) {
        perror("write TRANSACTION_FILE");
    }

    unlock_file(fd);
    close(fd);
    pthread_mutex_unlock(&txlog_mutex); // NEW
}


// --- Main Server (updated: init account mutexes) ---
int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1; socklen_t addrlen = sizeof(address);

    // Initialize the session array to all zeros
    memset(active_sessions, 0, sizeof(active_sessions));

    // NEW: Initialize all account mutexes
    for (int i = 0; i < MAX_ID; i++) {
        pthread_mutex_init(&account_mutexes[i], NULL);
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed"); exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt"); exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed"); exit(EXIT_FAILURE);
    }
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", SERVER_PORT);
    
    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            perror("accept"); continue;
        }
        
        pthread_t thread_id;
        int* client_sock_ptr = (int*)malloc(sizeof(int));
        if (!client_sock_ptr) { perror("malloc"); close(new_socket); continue; }
        *client_sock_ptr = new_socket;

        if (pthread_create(&thread_id, NULL, handle_client_connection, (void*)client_sock_ptr) != 0) {
            perror("pthread_create"); close(new_socket); free(client_sock_ptr); continue;
        }
        pthread_detach(thread_id);
    }
    close(server_fd); return 0;
}

// --- Thread Connection Handler (unchanged logic) ---
void* handle_client_connection(void* client_socket_ptr) {
    int sock_fd = *(int*)client_socket_ptr; free(client_socket_ptr);
    Request client_req; Response server_res;
    int user_id = -1; // -1 means no user is logged in on this thread
    UserRole user_role = -1;

    while (1) {
        int bytes = read(sock_fd, &client_req, sizeof(Request));
        if (bytes <= 0) {
            printf("Client disconnected (user %d).\n", user_id); break;
        }
        memset(&server_res, 0, sizeof(Response));
        client_req.user_id = user_id; 
        
        if (client_req.op == LOGIN) {
            handle_login(sock_fd, &client_req, &server_res);
            if (server_res.success) {
                user_id = server_res.data.user.id; // Thread now "owns" this user_id
                user_role = server_res.data.user.role;
            }
        } else if (client_req.op == EXIT) {
            break; // Will trigger session cleanup
        } else if (user_id == -1) {
            server_res.success = 0; strcpy(server_res.message, "Not logged in.");
        } 
        else if (client_req.op == CHANGE_PASSWORD) {
            handle_change_password(sock_fd, &client_req, &server_res);
        } 
        else { // User is logged in, route to role
            switch (user_role) {
                case CUSTOMER:
                    handle_customer_operations(sock_fd, &client_req, &server_res);
                    break;
                case EMPLOYEE:
                    handle_employee_operations(sock_fd, &client_req, &server_res);
                    break;
                case MANAGER:
                    handle_manager_operations(sock_fd, &client_req, &server_res);
                    break;
                case ADMIN:
                    handle_admin_operations(sock_fd, &client_req, &server_res);
                    break;
                default:
                    server_res.success = 0; strcpy(server_res.message, "Unknown user role.");
            }
        }
        if (write(sock_fd, &server_res, sizeof(Response)) <= 0) {
            printf("Write error to client %d.\n", user_id); break;
        }
    }
    
    // --- SESSION CLEANUP ---
    if (user_id != -1) { // Only if a user was successfully logged in
        pthread_mutex_lock(&session_lock);
        active_sessions[user_id] = 0; // Free the session
        pthread_mutex_unlock(&session_lock);
        printf("Session cleared for user %d.\n", user_id);
    }
    // --- END SESSION CLEANUP ---

    printf("Closing connection socket.\n");
    close(sock_fd); return NULL;
}

// --- Login Handler (unchanged logic) ---
void handle_login(int sock, Request* req, Response* res) {
    int fd_user = open(USER_FILE, O_RDONLY);
    if (fd_user == -1) { res->success = 0; strcpy(res->message, "Server DB error."); return; }
    User u;
    int user_id = atoi(req->username);
    if (user_id <= 0 || user_id >= 5000) { 
        res->success = 0; strcpy(res->message, "Invalid user ID format."); 
        close(fd_user); return; 
    }
    
    // Lock record to read password
    set_record_lock(fd_user, user_id, F_RDLCK, sizeof(User));
    lseek(fd_user, (off_t)user_id * (off_t)sizeof(User), SEEK_SET);
    int read_success = (read(fd_user, &u, sizeof(User)) == (ssize_t)sizeof(User));
    unlock_record(fd_user, user_id, sizeof(User));
    close(fd_user);

    if (read_success && u.id == user_id && strcmp(u.password, req->password) == 0) {
        
        // --- ROLE VALIDATION ---
        if (u.role != req->intended_role) {
            res->success = 0;
            strcpy(res->message, "Login failed: ID and password do not match the selected user type.");
            return;
        }
        // --- END ROLE VALIDATION ---
        
        else if (u.isActive) {
            // Check session
            pthread_mutex_lock(&session_lock);
            if (active_sessions[user_id] == 1) {
                res->success = 0;
                strcpy(res->message, "Login failed. Please log out from your other session to log in here.");
            } else {
                active_sessions[user_id] = 1;
                res->success = 1;
                strcpy(res->message, "Login successful!");
                res->data.user = u;
            }
            pthread_mutex_unlock(&session_lock);
        } else {
            res->success = 0; strcpy(res->message, "Account is deactivated.");
        }
    } else { res->success = 0; strcpy(res->message, "Invalid ID or password."); }
}

// --- Common: Change Password Handler (unchanged logic) ---
// --- Common: Change Password Handler ---
void handle_change_password(int sock, Request* req, Response* res) {
    int fd_user = open(USER_FILE, O_RDWR);
    if (fd_user == -1) { 
        res->success = 0; 
        strcpy(res->message, "Server DB error."); 
        return; 
    }
    
    User user;
    int user_id = req->user_id; // Get ID from the session
    
    // --- FIX: Add in-process lock for thread safety ---
    lock_account_one(user_id);
    
    // Cross-process lock
    set_record_lock(fd_user, user_id, F_WRLCK, sizeof(User));
    
    lseek(fd_user, (off_t)user_id * (off_t)sizeof(User), SEEK_SET);
    if (read(fd_user, &user, sizeof(User)) != (ssize_t)sizeof(User)) {
        res->success = 0;
        strcpy(res->message, "User record not found.");
    } else {
        // All good, update the password
        strncpy(user.password, req->data.new_password, 99);
        user.password[99] = '\0'; // Ensure null-terminated
        
        lseek(fd_user, (off_t)user_id * (off_t)sizeof(User), SEEK_SET);
        write(fd_user, &user, sizeof(User));
        
        res->success = 1;
        strcpy(res->message, "Password changed successfully.");
    }
    
    // Unlock in reverse order
    unlock_record(fd_user, user_id, sizeof(User));
    unlock_account_one(user_id);
    // --- END FIX ---
    
    close(fd_user);
}

// --- Customer Handler (updated: per-account mutexes and pair locking) ---
// --- Customer Handler (CORRECTED) ---
void handle_customer_operations(int sock, Request* req, Response* res) {
    int fd_account = -1, fd_loan = -1, fd_feedback = -1, fd_trans = -1;
    Account acc;
    int cust_id = req->user_id;
    
    // Open fd_account ONCE at the top
    if (req->op == CUST_VIEW_BALANCE || req->op == CUST_DEPOSIT || req->op == CUST_WITHDRAW || req->op == CUST_TRANSFER) {
        fd_account = open(ACCOUNT_FILE, O_RDWR);
        if (fd_account == -1) { res->success = 0; strcpy(res->message, "Server DB error."); return; }
    }
    
    switch (req->op) {
        case CUST_VIEW_BALANCE:
            lock_account_one(cust_id); 
            set_record_lock(fd_account, cust_id, F_RDLCK, sizeof(Account));
            lseek(fd_account, (off_t)cust_id * (off_t)sizeof(Account), SEEK_SET);
            read(fd_account, &acc, sizeof(Account));
            unlock_record(fd_account, cust_id, sizeof(Account));
            unlock_account_one(cust_id); 
            
            res->success = 1; 
            res->data.balance = acc.balance; 
            sprintf(res->message, "Balance: $%.2f", acc.balance);
            break;
            
        case CUST_DEPOSIT:
            lock_account_one(cust_id); 
            set_record_lock(fd_account, cust_id, F_WRLCK, sizeof(Account));
            lseek(fd_account, (off_t)cust_id * (off_t)sizeof(Account), SEEK_SET);
            read(fd_account, &acc, sizeof(Account));
            acc.balance += req->data.amount;
            lseek(fd_account, (off_t)cust_id * (off_t)sizeof(Account), SEEK_SET);
            write(fd_account, &acc, sizeof(Account));
            unlock_record(fd_account, cust_id, sizeof(Account));
            unlock_account_one(cust_id); 
            
            log_transaction(cust_id, "DEPOSIT", req->data.amount, acc.balance);
            res->success = 1; sprintf(res->message, "Deposit successful. New balance: $%.2f", acc.balance);
            break;

        case CUST_WITHDRAW:
            lock_account_one(cust_id); 
            set_record_lock(fd_account, cust_id, F_WRLCK, sizeof(Account));
            lseek(fd_account, (off_t)cust_id * (off_t)sizeof(Account), SEEK_SET);
            read(fd_account, &acc, sizeof(Account));
            if (acc.balance >= req->data.amount) {
                acc.balance -= req->data.amount;
                lseek(fd_account, (off_t)cust_id * (off_t)sizeof(Account), SEEK_SET);
                write(fd_account, &acc, sizeof(Account));
                unlock_record(fd_account, cust_id, sizeof(Account));
                unlock_account_one(cust_id); 
                
                log_transaction(cust_id, "WITHDRAW", req->data.amount, acc.balance);
                res->success = 1; sprintf(res->message, "Withdrawal successful. New balance: $%.2f", acc.balance);
            } else {
                unlock_record(fd_account, cust_id, sizeof(Account));
                unlock_account_one(cust_id); 
                res->success = 0; strcpy(res->message, "Insufficient funds.");
            }
            break;

        case CUST_TRANSFER: 
            { 
                int from_id = cust_id;
                int to_id = req->data.transfer.to_account_id;
                double amount = req->data.transfer.amount;

                // --- NEW FIX: Validate to_id is within mutex array bounds ---
                if (to_id <= 0 || to_id >= MAX_ID) {
                    res->success = 0; strcpy(res->message, "Transfer failed: Invalid recipient ID.");
                    break; // This break is safe, fd_account will be closed at the end
                }
                // --- END FIX ---

                int fd_user = open(USER_FILE, O_RDONLY);
                if (fd_user == -1) {
                    res->success = 0; strcpy(res->message, "Server DB error (user file).");
                    break; 
                }
                
                if (from_id == to_id) {
                    res->success = 0; strcpy(res->message, "Cannot transfer to self.");
                    close(fd_user); 
                    break; 
                }
                
                Account from_acc, to_acc;
                User to_user; 
                int read_from_ok, read_to_ok, read_user_ok;
                int success = 0; // Flag for logging

                lock_account_pair(from_id, to_id);

                if (from_id < to_id) {
                    set_record_lock(fd_account, from_id, F_WRLCK, sizeof(Account));
                    set_record_lock(fd_account, to_id,   F_WRLCK, sizeof(Account));
                } else {
                    set_record_lock(fd_account, to_id,   F_WRLCK, sizeof(Account));
                    set_record_lock(fd_account, from_id, F_WRLCK, sizeof(Account));
                }
                
                lseek(fd_account, (off_t)from_id * (off_t)sizeof(Account), SEEK_SET);
                read_from_ok = (read(fd_account, &from_acc, sizeof(Account)) == (ssize_t)sizeof(Account));
                
                lseek(fd_account, (off_t)to_id * (off_t)sizeof(Account), SEEK_SET);
                read_to_ok = (read(fd_account, &to_acc, sizeof(Account)) == (ssize_t)sizeof(Account));
                
                set_record_lock(fd_user, to_id, F_RDLCK, sizeof(User));
                lseek(fd_user, (off_t)to_id * (off_t)sizeof(User), SEEK_SET);
                read_user_ok = (read(fd_user, &to_user, sizeof(User)) == (ssize_t)sizeof(User));
                unlock_record(fd_user, to_id, sizeof(User));
                
                if (!read_from_ok) {
                    res->success = 0; strcpy(res->message, "Transfer failed: Sender account invalid.");
                } 
                else if (!read_to_ok) {
                    res->success = 0; strcpy(res->message, "Transfer failed: Recipient account invalid.");
                }
                else if (!read_user_ok || to_user.id != to_id) {
                    res->success = 0; strcpy(res->message, "Transfer failed: Recipient user not found.");
                }
                else if (to_user.isActive == 0) { 
                    res->success = 0; strcpy(res->message, "Transfer failed: Recipient's account is deactivated.");
                }
                else if (from_acc.account_id != from_id || to_acc.account_id != to_id) {
                     res->success = 0; strcpy(res->message, "Transfer failed: Account ID mismatch.");
                }
                else if (from_acc.balance < amount) {
                    res->success = 0; strcpy(res->message, "Insufficient funds for transfer.");
                }
                else {
                    from_acc.balance -= amount;
                    to_acc.balance += amount;
                    
                    lseek(fd_account, (off_t)from_id * (off_t)sizeof(Account), SEEK_SET);
                    write(fd_account, &from_acc, sizeof(Account));
                    
                    lseek(fd_account, (off_t)to_id * (off_t)sizeof(Account), SEEK_SET);
                    write(fd_account, &to_acc, sizeof(Account));
                    
                    success = 1; 
                    res->success = 1; sprintf(res->message, "Transfer successful. New balance: $%.2f", from_acc.balance);
                }
                
                if (from_id < to_id) {
                    unlock_record(fd_account, to_id,   sizeof(Account));
                    unlock_record(fd_account, from_id, sizeof(Account));
                } else {
                    unlock_record(fd_account, from_id, sizeof(Account));
                    unlock_record(fd_account, to_id,   sizeof(Account));
                }

                unlock_account_pair(from_id, to_id); 
                
                close(fd_user); // Close the user file

                if (success) {
                    log_transaction(from_id, "TRANSFER_OUT", amount, from_acc.balance);
                    log_transaction(to_id, "TRANSFER_IN",  amount, to_acc.balance);
                }
            }
            break;
            
        case CUST_APPLY_LOAN:
            fd_loan = open(LOAN_FILE, O_RDWR | O_APPEND | O_CREAT, 0644);
            if (fd_loan == -1) { res->success = 0; strcpy(res->message, "Server DB error."); break; }
            set_file_lock(fd_loan, F_WRLCK);
            off_t offset = lseek(fd_loan, 0, SEEK_END);
            int loan_id = (int)(offset / (off_t)sizeof(Loan)) + 1;
            Loan new_loan = {loan_id, cust_id, req->data.amount, "PENDING", 0};
            write(fd_loan, &new_loan, sizeof(Loan));
            unlock_file(fd_loan); close(fd_loan);
            res->success = 1; strcpy(res->message, "Loan application submitted.");
            break;

        case CUST_ADD_FEEDBACK: 
            fd_feedback = open(FEEDBACK_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
            if (fd_feedback == -1) { res->success = 0; strcpy(res->message, "Server DB error."); break; }
            set_file_lock(fd_feedback, F_WRLCK);
            off_t fb_offset = lseek(fd_feedback, 0, SEEK_END);
            int fb_id = (int)(fb_offset / (off_t)sizeof(Feedback)) + 1;
            Feedback new_fb = {fb_id, cust_id, "", time(NULL)};
            strncpy(new_fb.message, req->data.feedback_message, 511);
            new_fb.message[511] = '\0';
            write(fd_feedback, &new_fb, sizeof(Feedback));
            unlock_file(fd_feedback); close(fd_feedback);
            res->success = 1; strcpy(res->message, "Feedback submitted. Thank you!");
            break;

        case CUST_VIEW_HISTORY: 
            {
                fd_trans = open(TRANSACTION_FILE, O_RDONLY);
                if(fd_trans == -1) { res->success = 0; strcpy(res->message, "Server DB error."); break; }
                
                set_file_lock(fd_trans, F_RDLCK);
                
                Transaction tx;
                int count = 0;
                off_t f_offset = lseek(fd_trans, 0, SEEK_END);
                while(count < MAX_TRANSACTIONS && f_offset > 0) {
                    f_offset -= (off_t)sizeof(Transaction);
                    lseek(fd_trans, f_offset, SEEK_SET);
                    
                    if (read(fd_trans, &tx, sizeof(Transaction)) == (ssize_t)sizeof(Transaction)) {
                        if(tx.account_id == cust_id) {
                            res->data.tx_history.history[count] = tx;
                            count++;
                        }
                    }
                }
                
                unlock_file(fd_trans);
                close(fd_trans);
                
                res->success = 1;
                res->data.tx_history.history_count = count;
                sprintf(res->message, "Found %d transaction(s).", count);
            }
            break;
            
        default:
            res->success = 0; strcpy(res->message, "Unknown customer operation.");
    }
    
    // This is the ONLY place fd_account should be closed.
    if (fd_account != -1) close(fd_account);
}

// --- Employee Handler (unchanged) ---
void handle_employee_operations(int sock, Request* req, Response* res) {
    
    switch (req->op) {
        case EMP_ADD_CUSTOMER: 
            { 
                int fd_user = open(USER_FILE, O_RDWR | O_CREAT, 0644);
                int fd_account = open(ACCOUNT_FILE, O_RDWR | O_CREAT, 0644);
                if (fd_user == -1 || fd_account == -1) { res->success = 0; strcpy(res->message, "Server DB error."); break; }
                
                User user;
                set_file_lock(fd_user, F_WRLCK); 
                int new_cust_id = 1001;
                while(1) {
                    lseek(fd_user, (off_t)new_cust_id * (off_t)sizeof(User), SEEK_SET);
                    
                    if (read(fd_user, &user, sizeof(User)) <= 0) {
                        break; // Hit physical end of file, slot is free
                    }
                    if (user.id == 0) {
                        break; // Found an empty (zeroed) slot
                    }

                    new_cust_id++;
                    if (new_cust_id >= 2000) { 
                        res->success=0; strcpy(res->message, "No IDs available."); 
                        break;
                    }
                }

                if (new_cust_id >= 2000) {
                    unlock_file(fd_user); close(fd_user); close(fd_account);
                    return;
                }
                
                User new_cust = req->data.user_data;
                new_cust.id = new_cust_id; 
                new_cust.role = CUSTOMER; 
                new_cust.isActive = 1;
                sprintf(new_cust.username, "%d", new_cust_id);
                
                lseek(fd_user, (off_t)new_cust_id * (off_t)sizeof(User), SEEK_SET);
                write(fd_user, &new_cust, sizeof(User));
                unlock_file(fd_user); close(fd_user);
                
                Account acc = {new_cust_id, new_cust_id, 0.0};
                set_record_lock(fd_account, new_cust_id, F_WRLCK, sizeof(Account));
                lseek(fd_account, (off_t)new_cust_id * (off_t)sizeof(Account), SEEK_SET);
                write(fd_account, &acc, sizeof(Account));
                unlock_record(fd_account, new_cust_id, sizeof(Account)); close(fd_account);
                res->success = 1; sprintf(res->message, "Customer created. ID: %d", new_cust_id);
            }
            break;

        case EMP_MOD_CUSTOMER: 
            {
                int fd_user = open(USER_FILE, O_RDWR);
                if (fd_user == -1) { res->success = 0; strcpy(res->message, "Server DB error."); break; }
                
                int target_id = req->data.target_user_id;
                User user;
                
                set_record_lock(fd_user, target_id, F_WRLCK, sizeof(User));
                
                lseek(fd_user, (off_t)target_id * (off_t)sizeof(User), SEEK_SET);
                if (read(fd_user, &user, sizeof(User)) <= 0) {
                    res->success = 0;
                    strcpy(res->message, "User record not found.");
                } else if (user.role != CUSTOMER) {
                    res->success = 0;
                    strcpy(res->message, "Can only modify customer accounts.");
                } else {
                    strncpy(user.name, req->data.user_data.name, 99);
                    user.name[99] = '\0';
                    
                    lseek(fd_user, (off_t)target_id * (off_t)sizeof(User), SEEK_SET);
                    write(fd_user, &user, sizeof(User));
                    res->success = 1;
                    strcpy(res->message, "Customer details updated.");
                }
                unlock_record(fd_user, target_id, sizeof(User));
                close(fd_user);
            }
            break;
            
        case EMP_VIEW_ASSIGNED_LOANS: 
            {
                int fd_loan = open(LOAN_FILE, O_RDONLY);
                if (fd_loan == -1) { res->success=0; strcpy(res->message, "Server DB error."); break; }

                set_file_lock(fd_loan, F_RDLCK); 
                Loan loan;
                int count = 0;
                int employee_id = req->user_id; 
                
                while (read(fd_loan, &loan, sizeof(Loan)) == (ssize_t)sizeof(Loan)) {
                    if (strcmp(loan.status, "PENDING") == 0 && loan.assigned_to_employee_id == employee_id) {
                        if (count < 20) { 
                            res->data.loan_list.loans[count] = loan;
                        }
                        count++;
                    }
                }
                unlock_file(fd_loan); close(fd_loan);

                res->success = 1;
                res->data.loan_list.loan_count = count;
                sprintf(res->message, "Found %d assigned loan(s).", count);
            }
            break;

        case EMP_PROCESS_LOAN: 
            { 
                int fd_loan = open(LOAN_FILE, O_RDWR);
                int fd_account = -1;
                if (fd_loan == -1) { res->success = 0; strcpy(res->message, "Server DB error."); break; }

                Loan loan;
                Account acc;
                int loan_id_to_process = req->data.loan_action.loan_id;
                int loan_index = loan_id_to_process - 1; 

                set_record_lock(fd_loan, loan_index, F_WRLCK, sizeof(Loan));
                
                lseek(fd_loan, (off_t)loan_index * (off_t)sizeof(Loan), SEEK_SET); 
                if(read(fd_loan, &loan, sizeof(Loan)) <= 0 || loan.loan_id != loan_id_to_process) {
                     res->success = 0; strcpy(res->message, "Loan ID not found.");
                } 
                else if (loan.assigned_to_employee_id != req->user_id) {
                     res->success = 0; strcpy(res->message, "This loan is not assigned to you.");
                }
                else if (strcmp(loan.status, "PENDING") != 0) {
                     res->success = 0; strcpy(res->message, "Loan is not pending.");
                }
                else if (req->data.loan_action.approve) {
                    strcpy(loan.status, "APPROVED");
                    fd_account = open(ACCOUNT_FILE, O_RDWR);
                    set_record_lock(fd_account, loan.customer_id, F_WRLCK, sizeof(Account));
                    lseek(fd_account, (off_t)loan.customer_id * (off_t)sizeof(Account), SEEK_SET);
                    read(fd_account, &acc, sizeof(Account));
                    acc.balance += loan.amount;
                    lseek(fd_account, (off_t)loan.customer_id * (off_t)sizeof(Account), SEEK_SET);
                    write(fd_account, &acc, sizeof(Account));
                    unlock_record(fd_account, loan.customer_id, sizeof(Account));
                    close(fd_account);
                    log_transaction(loan.customer_id, "LOAN_DEPOSIT", loan.amount, acc.balance);
                    strcpy(res->message, "Loan approved and funds deposited.");
                    res->success = 1;
                } else {
                    strcpy(loan.status, "REJECTED");
                    strcpy(res->message, "Loan rejected.");
                    res->success = 1;
                }
                
                if(res->success) {
                    lseek(fd_loan, (off_t)loan_index * (off_t)sizeof(Loan), SEEK_SET);
                    write(fd_loan, &loan, sizeof(Loan));
                }
                
                unlock_record(fd_loan, loan_index, sizeof(Loan));
                close(fd_loan);
            }
            break;

        case EMP_VIEW_CUST_TX: 
            {
                int fd_trans = open(TRANSACTION_FILE, O_RDONLY);
                if(fd_trans == -1) { res->success = 0; strcpy(res->message, "Server DB error."); break; }
                
                int target_cust_id = req->data.target_user_id;
                
                set_file_lock(fd_trans, F_RDLCK);
                
                Transaction tx;
                int count = 0;
                off_t f_offset = lseek(fd_trans, 0, SEEK_END);
                while(count < MAX_TRANSACTIONS && f_offset > 0) {
                    f_offset -= (off_t)sizeof(Transaction);
                    lseek(fd_trans, f_offset, SEEK_SET);
                    
                    if (read(fd_trans, &tx, sizeof(Transaction)) == (ssize_t)sizeof(Transaction)) {
                        if(tx.account_id == target_cust_id) {
                            res->data.tx_history.history[count] = tx;
                            count++;
                        }
                    }
                }
                
                unlock_file(fd_trans);
                close(fd_trans);
                
                res->success = 1;
                res->data.tx_history.history_count = count;
                sprintf(res->message, "Found %d transaction(s) for user %d.", count, target_cust_id);
            }
            break;
            
        default:
            res->success = 0; strcpy(res->message, "Unknown employee operation.");
    }
}

// --- Manager Handler (unchanged) ---
void handle_manager_operations(int sock, Request* req, Response* res) {
    int fd_user, fd_loan, fd_feedback;
    User user; Loan loan; Feedback fb;
    
    switch (req->op) {
        case MGR_ACTIVATE_USER:
        case MGR_DEACTIVATE_USER:
            {
                int target_id = req->data.target_user_id;
                fd_user = open(USER_FILE, O_RDWR);
                if (fd_user == -1) { res->success=0; strcpy(res->message, "Server DB error."); break; }
                set_record_lock(fd_user, target_id, F_WRLCK, sizeof(User));
                lseek(fd_user, (off_t)target_id * (off_t)sizeof(User), SEEK_SET);
                if (read(fd_user, &user, sizeof(User)) <= 0) {
                    res->success = 0; strcpy(res->message, "User not found.");
                } else if (user.role != CUSTOMER) {
                     res->success = 0; strcpy(res->message, "Can only manage Customer accounts.");
                } else {
                    
                    if (req->op == MGR_ACTIVATE_USER) {
                        if (user.isActive == 1) {
                            res->success = 0; 
                            sprintf(res->message, "User %d is already activated.", target_id);
                        } else {
                            user.isActive = 1;
                            lseek(fd_user, (off_t)target_id * (off_t)sizeof(User), SEEK_SET);
                            write(fd_user, &user, sizeof(User));
                            res->success = 1;
                            sprintf(res->message, "User %d activated.", target_id);
                        }
                    } else { // MGR_DEACTIVATE_USER
                        if (user.isActive == 0) {
                            res->success = 0; 
                            sprintf(res->message, "User %d is already deactivated.", target_id);
                        } else {
                            user.isActive = 0;
                            lseek(fd_user, (off_t)target_id * (off_t)sizeof(User), SEEK_SET);
                            write(fd_user, &user, sizeof(User));
                            res->success = 1;
                            sprintf(res->message, "User %d deactivated.", target_id);
                        }
                    }
                }
                unlock_record(fd_user, target_id, sizeof(User));
                close(fd_user);
            }
            break;

        case MGR_ASSIGN_LOAN:
            { 
                int emp_id = req->data.loan_assignment.employee_id;
                
                // --- VALIDATION: Check if emp_id is a real Employee ---
                fd_user = open(USER_FILE, O_RDONLY);
                if (fd_user == -1) { res->success=0; strcpy(res->message, "Server DB error."); break; }
                
                set_record_lock(fd_user, emp_id, F_RDLCK, sizeof(User));
                lseek(fd_user, (off_t)emp_id * (off_t)sizeof(User), SEEK_SET);
                if (read(fd_user, &user, sizeof(User)) != (ssize_t)sizeof(User)) {
                    res->success = 0; strcpy(res->message, "Employee ID not found.");
                    unlock_record(fd_user, emp_id, sizeof(User));
                    close(fd_user);
                    break;
                }
                
                if (user.role != EMPLOYEE) {
                    res->success = 0; strcpy(res->message, "Invalid ID. You must assign to an Employee.");
                    unlock_record(fd_user, emp_id, sizeof(User));
                    close(fd_user);
                    break;
                }
                unlock_record(fd_user, emp_id, sizeof(User));
                close(fd_user);
                // --- END VALIDATION ---

                fd_loan = open(LOAN_FILE, O_RDWR);
                if (fd_loan == -1) { res->success=0; strcpy(res->message, "Server DB error."); break; }
                
                int loan_id = req->data.loan_assignment.loan_id;
                int loan_index = loan_id - 1; 
                
                set_record_lock(fd_loan, loan_index, F_WRLCK, sizeof(Loan));
                lseek(fd_loan, (off_t)loan_index * (off_t)sizeof(Loan), SEEK_SET);
                if (read(fd_loan, &loan, sizeof(Loan)) <= 0 || loan.loan_id != loan_id) {
                    res->success = 0; strcpy(res->message, "Loan not found.");
                } else if (strcmp(loan.status, "PENDING") != 0) {
                    res->success = 0; strcpy(res->message, "Loan is not pending.");
                } else {
                    loan.assigned_to_employee_id = emp_id;
                    lseek(fd_loan, (off_t)loan_index * (off_t)sizeof(Loan), SEEK_SET);
                    write(fd_loan, &loan, sizeof(Loan));
                    res->success = 1;
                    sprintf(res->message, "Loan %d assigned to employee %d.", loan_id, emp_id);
                }
                unlock_record(fd_loan, loan_index, sizeof(Loan));
                close(fd_loan);
            }
            break;

        case MGR_REVIEW_FEEDBACK:
            fd_feedback = open(FEEDBACK_FILE, O_RDONLY);
            if (fd_feedback == -1) { res->success=0; strcpy(res->message, "Server DB error."); break; }
            set_file_lock(fd_feedback, F_RDLCK);
            int count = 0;
            while(read(fd_feedback, &fb, sizeof(Feedback)) == (ssize_t)sizeof(Feedback) && count < 50) {
                res->data.feedback.list[count++] = fb;
            }
            unlock_file(fd_feedback); close(fd_feedback);
            res->data.feedback.count = count;
            res->success = 1;
            sprintf(res->message, "Found %d feedback entries.", count);
            break;

        case MGR_VIEW_PENDING_LOANS: 
            {
                int fd_loan = open(LOAN_FILE, O_RDONLY);
                if (fd_loan == -1) { res->success=0; strcpy(res->message, "Server DB error."); break; }
                set_file_lock(fd_loan, F_RDLCK);
                Loan loan;
                int count = 0;
                while (read(fd_loan, &loan, sizeof(Loan)) == (ssize_t)sizeof(Loan)) {
                    if (strcmp(loan.status, "PENDING") == 0) {
                        if (count < 20) { 
                            res->data.loan_list.loans[count] = loan;
                        }
                        count++;
                    }
                }
                unlock_file(fd_loan); close(fd_loan);
                res->success = 1;
                res->data.loan_list.loan_count = count;
                sprintf(res->message, "Found %d total pending loan(s).", count);
            }
            break;

        case MGR_VIEW_USER_LIST: 
            {
                fd_user = open(USER_FILE, O_RDONLY);
                if (fd_user == -1) { res->success=0; strcpy(res->message, "Server DB error."); break; }
                
                UserRole role_to_list = req->data.user_data.role;
                int count = 0;
                
                set_file_lock(fd_user, F_RDLCK);
                lseek(fd_user, 0, SEEK_SET); 
                
                while(read(fd_user, &user, sizeof(User)) == (ssize_t)sizeof(User)) {
                    if (user.id != 0 && user.role == role_to_list) {
                        if(count < MAX_USER_LIST) {
                            res->data.user_list.list[count] = user;
                        }
                        count++;
                    }
                }
                
                unlock_file(fd_user);
                close(fd_user);
                
                res->success = 1;
                res->data.user_list.count = count;
                sprintf(res->message, "Found %d user(s) of that type.", count);
            }
            break;

        default:
            res->success = 0; strcpy(res->message, "Unknown manager operation.");
    }
}

// --- Admin Handler (unchanged) ---
void handle_admin_operations(int sock, Request* req, Response* res) {
    int fd_user; 
    User user;
    
    switch (req->op) {
        case ADMIN_ADD_USER:
            fd_user = open(USER_FILE, O_RDWR | O_CREAT, 0644); 
            if (fd_user == -1) { res->success=0; strcpy(res->message, "Server DB error."); break; }
            set_file_lock(fd_user, F_WRLCK); 
            User new_user = req->data.user_data; 
            int start_id = 0, end_id = 0;
            
            if(new_user.role == CUSTOMER) { start_id = 1001; end_id = 1999; }
            else if(new_user.role == EMPLOYEE) { start_id = 2001; end_id = 2999; }
            else if(new_user.role == MANAGER) { start_id = 3001; end_id = 3999; }
            else if(new_user.role == ADMIN) { start_id = 4001; end_id = 4999; }
            else { res->success=0; strcpy(res->message, "Invalid role."); break; }

            int new_id = start_id;
            while(1) {
                lseek(fd_user, (off_t)new_id * (off_t)sizeof(User), SEEK_SET);
                
                if (read(fd_user, &user, sizeof(User)) <= 0) {
                    break; // Hit physical end of file, slot is free
                }
                if (user.id == 0) {
                    break; // Found an empty (zeroed) slot
                }
                
                new_id++;
                if (new_id > end_id) { res->success=0; strcpy(res->message, "No IDs available for this role."); break; }
            }
            
            if (new_id > end_id) { // Check if loop broke due to being full
                unlock_file(fd_user); close(fd_user);
                break;
            }

            new_user.id = new_id; 
            new_user.isActive = 1;
            sprintf(new_user.username, "%d", new_id);
            
            lseek(fd_user, (off_t)new_id * (off_t)sizeof(User), SEEK_SET);
            write(fd_user, &new_user, sizeof(User));
            
            if (new_user.role == CUSTOMER) {
                int fd_account = open(ACCOUNT_FILE, O_RDWR | O_CREAT, 0644);
                Account acc = {new_id, new_id, 0.0};
                set_record_lock(fd_account, new_id, F_WRLCK, sizeof(Account));
                lseek(fd_account, (off_t)new_id * (off_t)sizeof(Account), SEEK_SET);
                write(fd_account, &acc, sizeof(Account));
                unlock_record(fd_account, new_id, sizeof(Account));
                close(fd_account);
            }
            
            unlock_file(fd_user); close(fd_user);
            res->success = 1;
            sprintf(res->message, "User created. New ID: %d", new_id);
            break;
            
        case ADMIN_MOD_USER:
            {
                int target_id = req->data.target_user_id;
                fd_user = open(USER_FILE, O_RDWR);
                if (fd_user == -1) { res->success=0; strcpy(res->message, "Server DB error."); break; }
                set_record_lock(fd_user, target_id, F_WRLCK, sizeof(User));
                lseek(fd_user, (off_t)target_id * (off_t)sizeof(User), SEEK_SET);
                if (read(fd_user, &user, sizeof(User)) <= 0) {
                    res->success = 0; strcpy(res->message, "User not found.");
                } else {
                    User updated_data = req->data.user_data;
                    strcpy(user.name, updated_data.name);
                    strcpy(user.password, updated_data.password);
                    user.role = updated_data.role;
                    user.isActive = updated_data.isActive;
                    sprintf(user.username, "%d", user.id);
                    
                    lseek(fd_user, (off_t)target_id * (off_t)sizeof(User), SEEK_SET);
                    write(fd_user, &user, sizeof(User));
                    res->success = 1;
                    sprintf(res->message, "User %d updated.", target_id);
                }
                unlock_record(fd_user, target_id, sizeof(User));
                close(fd_user);
            }
            break;

        case ADMIN_VIEW_USER_LIST: 
            {
                fd_user = open(USER_FILE, O_RDONLY);
                if (fd_user == -1) { res->success=0; strcpy(res->message, "Server DB error."); break; }
                
                UserRole role_to_list = req->data.user_data.role;
                int count = 0;
                
                set_file_lock(fd_user, F_RDLCK);
                lseek(fd_user, 0, SEEK_SET); // Start from beginning
                
                while(read(fd_user, &user, sizeof(User)) == (ssize_t)sizeof(User)) {
                    if (user.id != 0 && user.role == role_to_list) {
                        if(count < MAX_USER_LIST) {
                            res->data.user_list.list[count] = user;
                        }
                        count++;
                    }
                }
                
                unlock_file(fd_user);
                close(fd_user);
                
                res->success = 1;
                res->data.user_list.count = count;
                sprintf(res->message, "Found %d user(s) of that type.", count);
            }
            break;

        default:
            res->success = 0; strcpy(res->message, "Unknown admin operation.");
    }
}