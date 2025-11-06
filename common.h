#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // For system calls: read, write, open, close, lseek, fcntl
#include <sys/socket.h> // For socket programming
#include <netinet/in.h> // For internet addresses
#include <arpa/inet.h>  // For inet_pton
#include <pthread.h>    // For threading
#include <fcntl.h>      // For file control (locking)
#include <errno.h>      // For error numbers
#include <sys/stat.h>   // For file modes
#include <time.h>       // For transactions

#define SERVER_PORT 8080
#define MAX_CLIENTS 20
#define MAX_TRANSACTIONS 50 
#define MAX_USER_LIST 50 // Max users to send in one list

// --- Database File Names ---
#define USER_FILE "db_users.dat"
#define ACCOUNT_FILE "db_accounts.dat"
#define TRANSACTION_FILE "db_transactions.dat"
#define LOAN_FILE "db_loans.dat"
#define FEEDBACK_FILE "db_feedback.dat" 

// --- Role Definitions ---
typedef enum {
    CUSTOMER = 1,
    EMPLOYEE = 2,
    MANAGER = 3,
    ADMIN = 4
} UserRole;

// --- Data Structures ---

// Stored in USER_FILE
typedef struct {
    int id; 
    UserRole role;
    char username[100];
    char password[100];
    char name[100]; 
    int isActive; 
} User;

// Stored in ACCOUNT_FILE
typedef struct {
    int account_id;     
    int customer_id;
    double balance;
} Account;

// Stored in TRANSACTION_FILE (append-only)
typedef struct {
    int transaction_id;
    int account_id;
    time_t timestamp;
    char type[20]; 
    double amount;
    double new_balance;
} Transaction;

// Stored in LOAN_FILE (append-only)
typedef struct {
    int loan_id;
    int customer_id;
    double amount;
    char status[20]; // "PENDING", "APPROVED", "REJECTED"
    int assigned_to_employee_id;
} Loan;

// Stored in FEEDBACK_FILE (append-only)
typedef struct {
    int feedback_id;
    int customer_id;
    char message[512];
    time_t timestamp;
} Feedback;


// --- Operation Codes for Client-Server Communication ---
typedef enum {
    // Shared
    LOGIN = 1,
    CHANGE_PASSWORD = 2, 
    LOGOUT = 3,
    EXIT = 4,

    // Customer operations
    CUST_VIEW_BALANCE = 11, 
    CUST_DEPOSIT = 12,      
    CUST_WITHDRAW = 13,     
    CUST_TRANSFER = 14,     
    CUST_APPLY_LOAN = 15,
    CUST_ADD_FEEDBACK = 17, 
    CUST_VIEW_HISTORY = 18, 

    // Employee operations
    EMP_ADD_CUSTOMER = 21,
    EMP_MOD_CUSTOMER = 22,      
    EMP_PROCESS_LOAN = 23,
    EMP_VIEW_CUST_TX = 24,      
    EMP_VIEW_ASSIGNED_LOANS = 25, 
    
    // Manager operations
    MGR_ACTIVATE_USER = 31,
    MGR_DEACTIVATE_USER = 32,
    MGR_ASSIGN_LOAN = 33,
    MGR_REVIEW_FEEDBACK = 34,
    MGR_VIEW_PENDING_LOANS = 35,
    MGR_VIEW_USER_LIST = 36,

    // Admin operations
    ADMIN_ADD_USER = 41, 
    ADMIN_MOD_USER = 42,
    ADMIN_DELETE_USER = 43, // Note: Delete is not implemented in server
    ADMIN_VIEW_USER_LIST = 44,

} Operation;

// --- Request/Response Structures ---

typedef struct {
    Operation op;
    int user_id;
    char username[100]; 
    char password[100]; 
    UserRole intended_role; // Used for login validation
    
    union {
        double amount; 
        struct {
            int to_account_id;
            double amount;
        } transfer;
        User user_data; // Also used to send *which role* to list
        int target_user_id; 
        struct {
            int loan_id;
            int employee_id;
        } loan_assignment;
         struct {
            int loan_id;
            int approve;
        } loan_action;
        char feedback_message[512];
        char new_password[100]; 
    } data;
} Request;

typedef struct {
    int success; 
    char message[256];
    
    union {
        User user; 
        double balance;
        
        // --- MODIFIED BLOCK ---
        // Each list is now bundled with its count
        struct {
            Transaction history[MAX_TRANSACTIONS]; 
            int history_count;
        } tx_history;
        
        struct {
            Loan loans[20]; 
            int loan_count;
        } loan_list;
        
        struct {
            Feedback list[50];
            int count;
        } feedback;
        
        struct {
            User list[MAX_USER_LIST]; 
            int count;                
        } user_list;
        // --- END MODIFIED BLOCK ---
        
    } data;
} Response;


// --- Helper Functions for File Locking (to be in server) ---
// These are declared here so server.c can use them, but they are
// not defined here. They are defined in server.c.
void set_record_lock(int fd, int record_id, int type, size_t struct_size);
void unlock_record(int fd, int record_id, size_t struct_size);

#endif // COMMON_H