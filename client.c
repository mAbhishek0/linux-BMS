#include "common.h"
#include <time.h> // Needed for ctime_r

// --- Function Prototypes ---
void handle_login_flow(int sock);
void clear_stdin_buffer();

// Common
void change_password(int sock);

// Customer
void show_customer_menu(int sock);
void view_balance(int sock);
void deposit_money(int sock);
void withdraw_money(int sock);
void transfer_funds(int sock);
void view_transaction_history(int sock);
void apply_for_loan(int sock);
void add_feedback(int sock);

// Employee
void show_employee_menu(int sock);
void add_new_customer(int sock);
void emp_modify_customer(int sock);
void emp_view_customer_tx(int sock);
void employee_process_loan(int sock); 

// Manager
void show_manager_menu(int sock);
void mgr_toggle_account(int sock, Operation op);
void mgr_view_pending_loans(int sock);
void mgr_assign_loan(int sock);
void mgr_review_feedback(int sock);
void mgr_view_user_list(int sock); 

// Admin
void show_admin_menu(int sock);
void admin_add_user(int sock);
void admin_mod_user(int sock);
void admin_view_user_list(int sock); 

// Helpers
void display_tx_history(Response* res);
void display_user_details(User* user); 


// --- Helper function to clear stdin buffer ---
void clear_stdin_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// --- Main ---
int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n"); return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    
    // !!! IMPORTANT !!!
    // Change "127.0.0.1" to your server's local IP address
    // if you are running the client on a different computer.
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address\n"); return -1;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed. Is the server running?\n"); return -1;
    }
    printf("Connected to Bank Server.\n");
    handle_login_flow(sock);
    printf("Thank you for using our bank. Goodbye.\n");
    close(sock);
    return 0;
}


// --- Login & Router ---
void handle_login_flow(int sock) {
    Request req; Response res;
    int choice;
    char id_prompt[50]; 

    while(1) {
        memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
        req.op = LOGIN;
        
        printf("\n--- Select User Type ---\n");
        printf("1. Customer\n");
        printf("2. Employee\n");
        printf("3. Manager\n");
        printf("4. Administrator\n");
        printf("5. Exit\n");
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            clear_stdin_buffer();
            continue;
        }
        clear_stdin_buffer();

        switch (choice) {
            case 1: 
                strcpy(id_prompt, "Enter Customer ID:"); 
                req.intended_role = CUSTOMER;
                break;
            case 2: 
                strcpy(id_prompt, "Enter Employee ID:"); 
                req.intended_role = EMPLOYEE;
                break;
            case 3: 
                strcpy(id_prompt, "Enter Manager ID:"); 
                req.intended_role = MANAGER;
                break;
            case 4: 
                strcpy(id_prompt, "Enter Admin ID:"); 
                req.intended_role = ADMIN;
                break;
            case 5: return; // Exit the login flow
            default:
                printf("Invalid choice. Please try again.\n");
                continue; 
        }
        
        printf("%s ", id_prompt); 
        scanf("%99s", req.username); 
        
        printf("Enter Password: "); 
        scanf("%99s", req.password);
        clear_stdin_buffer(); 
        
        if (write(sock, &req, sizeof(Request)) <= 0) {
            printf("Connection lost to server.\n"); return;
        }
        if (read(sock, &res, sizeof(Response)) <= 0) {
            printf("Connection lost to server.\n"); return;
        }
        
        printf("SERVER: %s\n", res.message);
        
        if (res.success) {
            switch (res.data.user.role) {
                case CUSTOMER: 
                    printf("\nWelcome Customer %s!\n", res.data.user.name);
                    show_customer_menu(sock); 
                    break;
                case EMPLOYEE: 
                    printf("\nWelcome Employee %s!\n", res.data.user.name);
                    show_employee_menu(sock); 
                    break;
                case MANAGER:  
                    printf("\nWelcome Manager %s!\n", res.data.user.name);
                    show_manager_menu(sock);  
                    break;
                case ADMIN:    
                    printf("\nWelcome Admin %s!\n", res.data.user.name);
                    show_admin_menu(sock);    
                    break;
                default: 
                    printf("Unknown role returned by server.\n");
            }
            break; 
        } else {
            // Server already sent the specific error
            printf("Please try again.\n");
        }
    }
    req.op = EXIT;
    write(sock, &req, sizeof(Request));
}

// =================================================
// ---            COMMON SECTION               ---
// =================================================

void change_password(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = CHANGE_PASSWORD;
    
    printf("Enter new password: ");
    scanf("%99s", req.data.new_password);
    clear_stdin_buffer();
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}

// =================================================
// ---            CUSTOMER SECTION             ---
// =================================================
void show_customer_menu(int sock) {
    int choice;
    while (1) {
        printf("\n--- Customer Menu ---\n");
        printf("1. View Balance\n");
        printf("2. Deposit Money\n");
        printf("3. Withdraw Money\n");
        printf("4. Transfer Funds\n");
        printf("5. View Transaction History\n");
        printf("6. Apply for Loan\n");
        printf("7. Add Feedback\n");
        printf("8. Change Password\n");
        printf("9. Logout\n");
        printf("Enter your choice: ");
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            clear_stdin_buffer(); 
            continue; 
        }
        clear_stdin_buffer(); 

        switch (choice) {
            case 1: view_balance(sock); break;
            case 2: deposit_money(sock); break;
            case 3: withdraw_money(sock); break;
            case 4: transfer_funds(sock); break;
            case 5: view_transaction_history(sock); break;
            case 6: apply_for_loan(sock); break;
            case 7: add_feedback(sock); break;
            case 8: change_password(sock); break;
            case 9: return; // Logout
            default: printf("Invalid choice.\n");
        }
    }
}

void view_balance(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = CUST_VIEW_BALANCE;
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}

void deposit_money(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = CUST_DEPOSIT;
    
    printf("Enter amount to deposit: ");
    if (scanf("%lf", &req.data.amount) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer();
    
    if (req.data.amount <= 0) {
        printf("Deposit must be a positive amount.\n");
        return;
    }
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}

void withdraw_money(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = CUST_WITHDRAW;
    
    printf("Enter amount to withdraw: ");
    if (scanf("%lf", &req.data.amount) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer();
    
    if (req.data.amount <= 0) {
        printf("Withdrawal must be a positive amount.\n");
        return;
    }
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}

void transfer_funds(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = CUST_TRANSFER;
    
    printf("Enter recipient Account ID: ");
    if (scanf("%d", &req.data.transfer.to_account_id) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer();
    
    printf("Enter amount to transfer: ");
    if (scanf("%lf", &req.data.transfer.amount) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer();
    
    if (req.data.transfer.amount <= 0) {
        printf("Transfer must be a positive amount.\n");
        return;
    }
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}

void view_transaction_history(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = CUST_VIEW_HISTORY;
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    
    printf("SERVER: %s\n", res.message);
    if(res.success && res.data.tx_history.history_count > 0) {
        display_tx_history(&res);
    }
}

void apply_for_loan(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = CUST_APPLY_LOAN;
    printf("Enter loan amount: "); 
    
    if (scanf("%lf", &req.data.amount) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer(); 
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}
void add_feedback(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = CUST_ADD_FEEDBACK;
    printf("Enter feedback (max 500 chars):\n");
    
    scanf(" %511[^\n]", req.data.feedback_message); 
    clear_stdin_buffer(); 
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}


// =================================================
// ---            EMPLOYEE SECTION             ---
// =================================================
void show_employee_menu(int sock) {
    int choice;
    while (1) {
        printf("\n--- Employee Menu ---\n");
        printf("1. Add New Customer\n");
        printf("2. Modify Customer Details\n");
        printf("3. View Customer Transactions (Passbook)\n");
        printf("4. View & Process Assigned Loans\n"); 
        printf("5. Change Password\n");
        printf("6. Logout\n");
        printf("Enter your choice: "); 
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            clear_stdin_buffer(); 
            continue; 
        }
        clear_stdin_buffer(); 
        
        switch (choice) {
            case 1: add_new_customer(sock); break;
            case 2: emp_modify_customer(sock); break;
            case 3: emp_view_customer_tx(sock); break;
            case 4: employee_process_loan(sock); break; 
            case 5: change_password(sock); break;
            case 6: return; // Logout
            default: printf("Invalid choice.\n");
        }
    }
}
void add_new_customer(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = EMP_ADD_CUSTOMER;
    printf("Enter new customer's full name: ");
    
    scanf(" %99[^\n]", req.data.user_data.name);
    clear_stdin_buffer(); 
    
    printf("Enter new customer's password: ");
    scanf("%99s", req.data.user_data.password);
    clear_stdin_buffer(); 
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}

void emp_modify_customer(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = EMP_MOD_CUSTOMER;
    
    printf("Enter Customer ID to modify: ");
    if (scanf("%d", &req.data.target_user_id) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer();
    
    printf("Enter customer's new full name: ");
    scanf(" %99[^\n]", req.data.user_data.name);
    clear_stdin_buffer();
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}

void emp_view_customer_tx(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = EMP_VIEW_CUST_TX;
    
    printf("Enter Customer ID to view transactions: ");
    if (scanf("%d", &req.data.target_user_id) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer();
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    
    printf("SERVER: %s\n", res.message);
    if(res.success && res.data.tx_history.history_count > 0) {
        display_tx_history(&res);
    }
}

void employee_process_loan(int sock) {
    Request req; Response res;
    int action;
    int loan_id_to_process;

    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = EMP_VIEW_ASSIGNED_LOANS; 
    
    write(sock, &req, sizeof(Request));
    if(read(sock, &res, sizeof(Response)) <= 0) {
        printf("Server disconnected.\n"); return;
    }
    
    printf("SERVER: %s\n", res.message);
    if (!res.success) return; 

    if (res.data.loan_list.loan_count == 0) {
        printf("No assigned loan applications to process.\n");
        return; 
    }

    printf("--- Assigned Pending Loan Applications ---\n");
    int max_to_show = (res.data.loan_list.loan_count > 20) ? 20 : res.data.loan_list.loan_count;
    for (int i = 0; i < max_to_show; i++) {
        Loan loan = res.data.loan_list.loans[i];
        printf("  Loan ID: %d | Customer ID: %d | Amount: $%.2f\n",
               loan.loan_id, loan.customer_id, loan.amount);
    }
    printf("----------------------------------------\n");

    memset(&req, 0, sizeof(req)); 
    req.op = EMP_PROCESS_LOAN;

    printf("Enter Loan ID to process (0 to cancel): ");
    if (scanf("%d", &loan_id_to_process) != 1) {
        printf("Invalid input. Please enter a number.\n");
        clear_stdin_buffer();
        return;
    }
    clear_stdin_buffer(); 
    if (loan_id_to_process == 0) return;
    req.data.loan_action.loan_id = loan_id_to_process;

    printf("Enter 1 to Approve, 0 to Reject: ");
    if (scanf("%d", &action) != 1) {
        printf("Invalid input. Please enter a number.\n");
        clear_stdin_buffer();
        return;
    }
    clear_stdin_buffer(); 

    if (action != 0 && action != 1) {
        printf("Invalid action. Must be 0 or 1.\n");
        return;
    }
    
    req.data.loan_action.approve = action;
    
    write(sock, &req, sizeof(Request));
    if(read(sock, &res, sizeof(Response)) <= 0) {
         printf("Server disconnected.\n"); return;
    }
    printf("SERVER: %s\n", res.message);
}


// =================================================
// ---            MANAGER SECTION              ---
// =================================================
void show_manager_menu(int sock) {
    int choice;
    while (1) {
        printf("\n--- Manager Menu ---\n");
        printf("1. Activate Customer Account\n");
        printf("2. Deactivate Customer Account\n");
        printf("3. View All Pending Loans\n");
        printf("4. Assign Loan to Employee\n");
        printf("5. Review Customer Feedback\n");
        printf("6. View User Details\n"); 
        printf("7. Change Password\n");
        printf("8. Logout\n");
        printf("Enter your choice: "); 
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            clear_stdin_buffer(); 
            continue; 
        }
        clear_stdin_buffer(); 
        
        switch (choice) {
            case 1: mgr_toggle_account(sock, MGR_ACTIVATE_USER); break;
            case 2: mgr_toggle_account(sock, MGR_DEACTIVATE_USER); break;
            case 3: mgr_view_pending_loans(sock); break;
            case 4: mgr_assign_loan(sock); break;
            case 5: mgr_review_feedback(sock); break;
            case 6: mgr_view_user_list(sock); break; 
            case 7: change_password(sock); break;
            case 8: return; // Logout
            default: printf("Invalid choice.\n");
        }
    }
}
void mgr_toggle_account(int sock, Operation op) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = op;
    printf("Enter Customer ID to %s: ", (op == MGR_ACTIVATE_USER ? "activate" : "deactivate"));
    
    if (scanf("%d", &req.data.target_user_id) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer(); 
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}

void mgr_view_pending_loans(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = MGR_VIEW_PENDING_LOANS;
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    
    printf("SERVER: %s\n", res.message);
    if (res.success && res.data.loan_list.loan_count > 0) {
        printf("--- All Pending Loan Applications ---\n");
        int max_to_show = (res.data.loan_list.loan_count > 20) ? 20 : res.data.loan_list.loan_count;
        for (int i = 0; i < max_to_show; i++) {
            Loan loan = res.data.loan_list.loans[i];
            printf("  Loan ID: %d | Cust ID: %d | Amount: $%.2f | Assigned to: %d\n",
                   loan.loan_id, loan.customer_id, loan.amount, loan.assigned_to_employee_id);
        }
        printf("-----------------------------------\n");
    }
}

void mgr_assign_loan(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = MGR_ASSIGN_LOAN;
    
    printf("Enter Loan ID to assign: ");
    if (scanf("%d", &req.data.loan_assignment.loan_id) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer(); 
    
    printf("Enter Employee ID to assign to: ");
    if (scanf("%d", &req.data.loan_assignment.employee_id) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer(); 
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}
void mgr_review_feedback(int sock) {
    Request req; Response res;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = MGR_REVIEW_FEEDBACK;
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
    if (res.success && res.data.feedback.count > 0) {
        printf("--- Displaying %d Feedback Entries ---\n", res.data.feedback.count);
        for (int i = 0; i < res.data.feedback.count; i++) {
            Feedback fb = res.data.feedback.list[i];
            
            char time_str[26];
            ctime_r(&fb.timestamp, time_str);
            time_str[strlen(time_str) - 1] = '\0'; 
            
            printf("ID: %d | From: %d | Time: %s\nMessage: %s\n---\n",
                   fb.feedback_id, fb.customer_id, time_str, fb.message);
        }
    }
}

void mgr_view_user_list(int sock) { 
    Request req; Response res;
    int choice;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = MGR_VIEW_USER_LIST;

    printf("\n--- View User Details ---\n");
    printf("1. Customer\n");
    printf("2. Employee\n");
    printf("3. Manager\n");
    printf("Enter user type to view (1-3): ");
    
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > 3) {
         printf("Invalid input. Please select 1, 2, or 3.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer();
    
    req.data.user_data.role = (UserRole)choice;
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    
    printf("SERVER: %s\n", res.message);
    if (res.success && res.data.user_list.count > 0) {
        printf("--- Displaying %d User(s) ---\n", res.data.user_list.count);
        int max_to_show = (res.data.user_list.count > MAX_USER_LIST) ? MAX_USER_LIST : res.data.user_list.count;
        for (int i = 0; i < max_to_show; i++) {
            display_user_details(&res.data.user_list.list[i]);
        }
    }
}

// =================================================
// ---            ADMIN SECTION                ---
// =================================================
void show_admin_menu(int sock) {
    int choice;
    while (1) {
        printf("\n--- Administrator Menu ---\n");
        printf("1. Add New User (C/E/M/A)\n");
        printf("2. Modify User Details\n");
        printf("3. View User Details\n"); 
        printf("4. Change Password\n");
        printf("5. Logout\n");
        printf("Enter your choice: "); 
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            clear_stdin_buffer(); 
            continue; 
        }
        clear_stdin_buffer(); 
        
        switch (choice) {
            case 1: admin_add_user(sock); break;
            case 2: admin_mod_user(sock); break;
            case 3: admin_view_user_list(sock); break; 
            case 4: change_password(sock); break;
            case 5: return; // Logout
            default: printf("Invalid choice.\n");
        }
    }
}
void admin_add_user(int sock) {
    Request req; Response res;
    int role_choice;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = ADMIN_ADD_USER;
    
    printf("Enter role (1=Cust, 2=Emp, 3=Mgr, 4=Admin): "); 
    if (scanf("%d", &role_choice) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer(); 
    if(role_choice < 1 || role_choice > 4) {
        printf("Invalid role.\n"); return;
    }
    
    req.data.user_data.role = (UserRole)role_choice;
    printf("Enter new user's full name: ");
    
    scanf(" %99[^\n]", req.data.user_data.name);
    clear_stdin_buffer(); 
    
    printf("Enter new user's password: ");
    scanf("%99s", req.data.user_data.password);
    clear_stdin_buffer(); 
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}
void admin_mod_user(int sock) {
    Request req; Response res;
    int role_choice, active_choice;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = ADMIN_MOD_USER;
    
    printf("Enter User ID to modify: ");
    if (scanf("%d", &req.data.target_user_id) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer(); 
    
    printf("Enter new full name: ");
    scanf(" %99[^\n]", req.data.user_data.name); 
    clear_stdin_buffer(); 
    
    printf("Enter new password: ");
    scanf("%99s", req.data.user_data.password); 
    clear_stdin_buffer(); 
    
    printf("Enter new role (1=C, 2=E, 3=M, 4=A): "); 
    if (scanf("%d", &role_choice) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer(); 
    req.data.user_data.role = (UserRole)role_choice;
    
    printf("Enter active status (1=Active, 0=Inactive): "); 
    if (scanf("%d", &active_choice) != 1) {
         printf("Invalid input. Please enter a number.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer(); 
    req.data.user_data.isActive = active_choice;
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    printf("SERVER: %s\n", res.message);
}

void admin_view_user_list(int sock) {
    Request req; Response res;
    int choice;
    memset(&req, 0, sizeof(req)); memset(&res, 0, sizeof(res));
    req.op = ADMIN_VIEW_USER_LIST;

    printf("\n--- View User Details ---\n");
    printf("1. Customer\n");
    printf("2. Employee\n");
    printf("3. Manager\n");
    printf("4. Admin\n");
    printf("Enter user type to view (1-4): ");
    
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > 4) {
         printf("Invalid input.\n");
         clear_stdin_buffer();
         return; 
    }
    clear_stdin_buffer();
    
    req.data.user_data.role = (UserRole)choice;
    
    write(sock, &req, sizeof(Request));
    read(sock, &res, sizeof(Response));
    
    printf("SERVER: %s\n", res.message);
    if (res.success && res.data.user_list.count > 0) {
        printf("--- Displaying %d User(s) ---\n", res.data.user_list.count);
        int max_to_show = (res.data.user_list.count > MAX_USER_LIST) ? MAX_USER_LIST : res.data.user_list.count;
        for (int i = 0; i < max_to_show; i++) {
            display_user_details(&res.data.user_list.list[i]);
        }
    }
}


// --- HELPER to display transaction history ---
void display_tx_history(Response* res) {
    printf("--- Transaction History ---\n");
    for (int i = 0; i < res->data.tx_history.history_count; i++) {
        Transaction tx = res->data.tx_history.history[i];
        
        char time_str[26];
        ctime_r(&tx.timestamp, time_str);
        time_str[strlen(time_str) - 1] = '\0'; // Remove \n
        
        printf("  [%s]\n", time_str);
        printf("  ID: %d | Type: %s | Amount: $%.2f\n", tx.transaction_id, tx.type, tx.amount);
        printf("  New Balance: $%.2f\n", tx.new_balance);
        printf("  -------------------------\n");
    }
}

// --- HELPER to display user details ---
void display_user_details(User* user) {
    printf("  --- User Details ---\n");
    printf("  ID:     %d\n", user->id);
    printf("  Name:   %s\n", user->name);
    
    char role_str[20];
    switch(user->role) {
        case CUSTOMER: strcpy(role_str, "Customer"); break;
        case EMPLOYEE: strcpy(role_str, "Employee"); break;
        case MANAGER:  strcpy(role_str, "Manager"); break;
        case ADMIN:    strcpy(role_str, "Admin"); break;
        default:        strcpy(role_str, "Unknown"); break;
    }
    printf("  Role:   %s\n", role_str);
    printf("  Status: %s\n", user->isActive ? "Active" : "Deactivated");
    printf("  --------------------\n");
}