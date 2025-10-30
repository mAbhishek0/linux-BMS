#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include "bankDatabase.h"
#include "common.h"
#include "lock_util.h"
#include "utils.h"

#define CUSTOMER_FILE "customers.dat"
#define EMPLOYEE_FILE "employees.dat"

void add_new_employee(int client_socket) {
    struct Employee employee;  // Using Customer struct for employee data
    char buffer[1024];
    //printf("Enter Employee ID: ");
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "Enter Employee ID: ");
    //scanf("%d", &employee.id);
    send(client_socket, buffer, strlen(buffer), 0);
    recv(client_socket, buffer, sizeof(buffer), 0);
    employee.id = atoi(buffer);
    //printf("Enter Name: ");
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "Enter Name: ");
    send(client_socket, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    recv(client_socket, buffer, sizeof(buffer), 0);
    strcpy(employee.name, buffer);
    printf("Name..%s\n",employee.name);
    //scanf("%s", employee.name);
    //printf("Enter Password: ");
    //scanf("%s", employee.password);
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "Enter Password: ");
    send(client_socket, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    recv(client_socket, buffer, sizeof(buffer), 0);


    unsigned long hashed_input_password = hash_password(buffer);
    printf("password..%d\n", hashed_input_password);
    employee.password = hashed_input_password;
    printf("Hashed Password...%lu\n", employee.password);
    strcpy(employee.role, "Employee");
    employee.isActive = 1;

    int fd = open(EMPLOYEE_FILE, O_WRONLY | O_APPEND);
    if (fd == -1) {
        perror("Error opening employee file");
        memset(buffer, 0, sizeof(buffer));
        strcpy(buffer, "Error opening employee file");
        send(client_socket, buffer, strlen(buffer), 0);
        return;
    }

    lock_file(fd, F_WRLCK);
    write(fd, &employee, sizeof(employee));
    lock_file(fd, F_UNLCK);
    close(fd);

    printf("New employee added successfully!\n");
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "New employee added successfully!\n");
    send(client_socket, buffer, strlen(buffer), 0);
    close(fd);
}

void modify_user_details(int client_socket, const char *filename) {
    int id;
    struct Customer user;
    char buffer[1024];
    // printf("Enter User ID to modify: ");
    // scanf("%d", &id);
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "Enter Employee ID: ");
    send(client_socket, buffer, strlen(buffer), 0);
    recv(client_socket, buffer, sizeof(buffer), 0);

    id = atoi(buffer);
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        perror("Error opening file");
        memset(buffer, 0, sizeof(buffer));
        strcpy(buffer, "Error opening file");
        send(client_socket, buffer, strlen(buffer), 0);
        return;
    }

    lock_file(fd, F_WRLCK);
    while (read(fd, &user, sizeof(user)) > 0) {
        if (user.id == id) {
            // printf("Enter New Name: ");
            // scanf("%s", user.name);
            // printf("Enter New Password: ");
            // scanf("%s", user.password);
            memset(buffer, 0, sizeof(buffer));
            strcpy(buffer, "Enter New Name: ");
            send(client_socket, buffer, strlen(buffer), 0);
            recv(client_socket, buffer, sizeof(buffer), 0);
            strcpy(user.name, buffer);

            memset(buffer, 0, sizeof(buffer));
            strcpy(buffer, "Enter New Password: ");
            send(client_socket, buffer, strlen(buffer), 0);
            recv(client_socket, buffer, sizeof(buffer), 0);
            unsigned long hashed_input_password = hash_password(buffer);
            user.password = hashed_input_password;

            lseek(fd, -sizeof(user), SEEK_CUR);  // Move file pointer back
            write(fd, &user, sizeof(user));
            memset(buffer, 0, sizeof(buffer));
            strcpy(buffer, "User Details Updated Successfully!");
            send(client_socket, buffer, strlen(buffer), 0);
            lock_file(fd, F_UNLCK);
            close(fd);
            return;
        }
    }
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "User not found!");
    send(client_socket, buffer, strlen(buffer), 0);
    close(fd);
}

void change_user_password(int client_socket, const char *filename) {
    int id;
    struct Customer user;
    char buffer[1024];
    // printf("Enter User ID to change password: ");
    // scanf("%d", &id);
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "Enter User ID to change password:");
    send(client_socket, buffer, strlen(buffer), 0);
    recv(client_socket, buffer, sizeof(buffer), 0);
    id = atoi(buffer);

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        perror("Error opening file");
        memset(buffer, 0, sizeof(buffer));
        strcpy(buffer, "Error opening file");
        send(client_socket, buffer, strlen(buffer), 0);
        return;
    }

    lock_file(fd, F_WRLCK);
    while (read(fd, &user, sizeof(user)) > 0) {
        if (user.id == id) {
            // printf("Enter New Password: ");
            // scanf("%s", user.password);
            memset(buffer, 0, sizeof(buffer));
            strcpy(buffer, "Enter New Password:");
            send(client_socket, buffer, strlen(buffer), 0);
            recv(client_socket, buffer, sizeof(buffer), 0);
            //strcpy(user.password, buffer);
            unsigned long hashed_input_password = hash_password(buffer);
            user.password = hashed_input_password;

            lseek(fd, -sizeof(user), SEEK_CUR);
            write(fd, &user, sizeof(user));
            //printf("Password changed successfully!\n");
            memset(buffer, 0, sizeof(buffer));
            strcpy(buffer, "Password changed successfully!");
            send(client_socket, buffer, strlen(buffer), 0);
            lock_file(fd, F_UNLCK);
            close(fd);
            return;
        }
    }
    //printf("User not found!\n");
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "User not found!");
    send(client_socket, buffer, strlen(buffer), 0);
    lock_file(fd, F_UNLCK);
    close(fd);
}

void assign_role(int client_socket) {
    int id;
    char role[10];
    struct Employee employee;
    char buffer[1024];
    // printf("Enter Employee ID to assign role: ");
    // scanf("%d", &id);
    // printf("Enter Role (Manager/Employee): ");
    // scanf("%s", role);
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "Enter Employee ID to assign role:");
    send(client_socket, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    recv(client_socket, buffer, sizeof(buffer), 0);
    id = atoi(buffer);

    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "Enter Role (Manager/Employee):");
    send(client_socket, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    recv(client_socket, buffer, sizeof(buffer), 0);
    strcpy(role, buffer);

    int fd = open(EMPLOYEE_FILE, O_RDWR);
    if (fd == -1) {
        perror("Error opening employee file");
        memset(buffer, 0, sizeof(buffer));
        strcpy(buffer, "Error opening employee file");
        send(client_socket, buffer, strlen(buffer), 0);
        return;
    }

    lock_file(fd, F_WRLCK);
    while (read(fd, &employee, sizeof(employee)) > 0) {
        if (employee.id == id) {
            strcpy(employee.role, role);  // Using the password field as role for simplicity

            lseek(fd, -sizeof(employee), SEEK_CUR);
            write(fd, &employee, sizeof(employee));
            printf("Role assigned successfully!\n");
            memset(buffer, 0, sizeof(buffer));
            strcpy(buffer, "Role assigned successfully!");
            send(client_socket, buffer, strlen(buffer), 0);
            lock_file(fd, F_UNLCK);
            close(fd);
            return;
        }
    }
    printf("Employee not found!\n");
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "Employee not found!\n");
    send(client_socket, buffer, strlen(buffer), 0);
    lock_file(fd, F_UNLCK);
    close(fd);
}


