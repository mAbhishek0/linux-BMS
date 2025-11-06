#include "common.h"

// This utility creates the database files with initial data.
int main() {
    int fd;

    // --- Create Users ---
    fd = open(USER_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("open USER_FILE"); exit(EXIT_FAILURE); }

    User cust1 = {1001, CUSTOMER, "1001", "pass", "Alice Smith (Cust)", 1};
    User cust2 = {1002, CUSTOMER, "1002", "pass", "Bob Johnson (Cust)", 1};
    User emp1 = {2001, EMPLOYEE, "2001", "pass", "Charles Brown (Emp)", 1};
    User mgr1 = {3001, MANAGER, "3001", "pass", "David Lee (Mgr)", 1};
    User admin1 = {4001, ADMIN, "4001", "pass", "Eve White (Admin)", 1};

    // Use lseek to write records at their ID's position (sparse file)
    lseek(fd, 1001 * sizeof(User), SEEK_SET); write(fd, &cust1, sizeof(User));
    lseek(fd, 1002 * sizeof(User), SEEK_SET); write(fd, &cust2, sizeof(User));
    lseek(fd, 2001 * sizeof(User), SEEK_SET); write(fd, &emp1, sizeof(User));
    lseek(fd, 3001 * sizeof(User), SEEK_SET); write(fd, &mgr1, sizeof(User));
    lseek(fd, 4001 * sizeof(User), SEEK_SET); write(fd, &admin1, sizeof(User));
    close(fd);
    printf("User database created.\n");

    // --- Create Accounts ---
    fd = open(ACCOUNT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("open ACCOUNT_FILE"); exit(EXIT_FAILURE); }
    Account acc1 = {1001, 1001, 10000.00}; // Account for cust1
    Account acc2 = {1002, 1002, 5000.00};  // Account for cust2
    lseek(fd, 1001 * sizeof(Account), SEEK_SET); write(fd, &acc1, sizeof(Account));
    lseek(fd, 1002 * sizeof(Account), SEEK_SET); write(fd, &acc2, sizeof(Account));
    close(fd);
    printf("Account database created.\n");

    // --- Create Empty Transaction, Loan, and Feedback Files ---
    fd = open(TRANSACTION_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644); close(fd);
    printf("Transaction log created.\n");
    fd = open(LOAN_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644); close(fd);
    printf("Loan database created.\n");
    fd = open(FEEDBACK_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644); close(fd);
    printf("Feedback database created.\n");
    
    return 0;
}