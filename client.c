// Academia Portal Client
// CS-513 System Software Mini Project

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <termios.h>
#include <stdbool.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"
#define MAX_USERNAME_LENGTH 100
#define MAX_PASSWORD_LENGTH 100
#define MAX_COURSE_NAME_LENGTH 100
#define MAX_RESPONSE_LENGTH 1024

// Global variables
int client_fd = -1;
bool is_logged_in = false;
char current_user_type[20] = "";
int current_user_id = -1;

// Function prototypes
void connectToServer();
void loginMenu();
void adminMenu();
void studentMenu();
void facultyMenu();
void sendRequest(const char* request, char* response);
void cleanExit(int signal_num);
void getPasswordInput(char* password);

// Helper functions for UI
void clearScreen() {
    printf("\033[2J\033[1;1H"); // ANSI escape sequence to clear screen
}

void displayTitle(const char* title) {
    printf("====================================\n");
    printf("      %s\n", title);
    printf("====================================\n");
}

void displayError(const char* message) {
    printf("\033[31m%s\033[0m\n", message); // Red text
    printf("Press Enter to continue...");
    getchar(); // Wait for Enter key
}

void displaySuccess(const char* message) {
    printf("\033[32m%s\033[0m\n", message); // Green text
    printf("Press Enter to continue...");
    getchar(); // Wait for Enter key
}

void waitForEnter() {
    printf("\nPress Enter to continue...");
    getchar(); // Wait for Enter key
}

// Signal handler for cleanup
void cleanExit(int signal_num) {
    printf("\nExiting client application...\n");
    if (client_fd != -1) {
        close(client_fd);
    }
    exit(signal_num);
}

// Get password without showing it on screen
void getPasswordInput(char* password) {
    struct termios old_tio, new_tio;
    
    // Get terminal settings
    tcgetattr(STDIN_FILENO, &old_tio);
    
    // Copy settings
    new_tio = old_tio;
    
    // Disable echo
    new_tio.c_lflag &= (~ECHO);
    
    // Set new terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    
    // Read password (safely)
    if (fgets(password, MAX_PASSWORD_LENGTH, stdin) != NULL) {
        // Remove trailing newline if present
        size_t len = strlen(password);
        if (len > 0 && password[len-1] == '\n') {
            password[len-1] = '\0';
        }
    }
    
    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}

// Connect to the server
void connectToServer() {
    // Create socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        fprintf(stderr, "Socket creation error\n");
        exit(EXIT_FAILURE);
    }
    
    // Server address configuration
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    // Convert IP address from string to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address or address not supported\n");
        exit(EXIT_FAILURE);
    }
    
    // Connect to server
    if (connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Connection failed: Server might be offline\n");
        exit(EXIT_FAILURE);
    }
    printf("Connected to Academia Portal Server\n");
}

// Send request to server and get response
void sendRequest(const char* request, char* response) {
    // Initialize response buffer
    memset(response, 0, BUFFER_SIZE);
    
    // Send request to server
    if (send(client_fd, request, strlen(request), 0) < 0) {
        fprintf(stderr, "Failed to send request\n");
        strcpy(response, "ERROR");
        return;
    }
    
    // Receive response from server
    ssize_t bytesRead = read(client_fd, response, BUFFER_SIZE - 1);
    
    if (bytesRead < 0) {
        fprintf(stderr, "Failed to read response\n");
        strcpy(response, "ERROR");
        return;
    }
    
    // Ensure null termination
    response[bytesRead] = '\0';
}

// Extract token from string
void extractToken(const char* str, char* result, int token_index) {
    char temp[BUFFER_SIZE];
    strcpy(temp, str);
    
    char* token = strtok(temp, " ");
    int i = 0;
    
    while (token != NULL && i < token_index) {
        token = strtok(NULL, " ");
        i++;
    }
    
    if (token != NULL) {
        strcpy(result, token);
    } else {
        result[0] = '\0';
    }
 }
 
 // Login menu
void loginMenu() {
    while (!is_logged_in) {
        clearScreen();
        displayTitle("Academia Portal - Login");
        
        char username[MAX_USERNAME_LENGTH];
        char password[MAX_PASSWORD_LENGTH];
        char request[BUFFER_SIZE];
        char response[BUFFER_SIZE];
        
        printf("Username: ");
        scanf("%s", username);
        getchar(); // Clear input buffer
        
        printf("Password: ");
        getPasswordInput(password);
        
        // Send login request
        snprintf(request, BUFFER_SIZE, "LOGIN %s %s", username, password);
        sendRequest(request, response);
        
        // Process response
        if (strncmp(response, "LOGIN_SUCCESS", 13) == 0) {
            is_logged_in = true;
            
            // Extract user type
            char user_type[20];
            extractToken(response, user_type, 1);
            strcpy(current_user_type, user_type);
            
            // Extract user ID
            char user_id[10];
            extractToken(response, user_id, 2);
            current_user_id = atoi(user_id);
            
            displaySuccess("Login successful!");
        } else {
            // Extract error message
            char error_msg[BUFFER_SIZE];
            strcpy(error_msg, "Login failed: ");
            
            // Find position after first space
            char* pos = strchr(response, ' ');
            if (pos != NULL) {
                strcat(error_msg, pos + 1);
            } else {
                strcat(error_msg, "Unknown error");
            }
            
            displayError(error_msg);
        }
    }
    
    // Redirect to appropriate menu based on user type
    if (strcmp(current_user_type, "ADMIN") == 0) {
        adminMenu();
    } else if (strcmp(current_user_type, "STUDENT") == 0) {
        studentMenu();
    } else if (strcmp(current_user_type, "FACULTY") == 0) {
        facultyMenu();
    }
}

// Admin menu
void adminMenu() {
    while (is_logged_in) {
        clearScreen();
        displayTitle("Admin Dashboard");
        
        printf("1. Add Student\n");
        printf("2. Add Faculty\n");
        printf("3. Add Admin\n");
        printf("4. Activate/Deactivate Student\n");
        printf("5. Update Student/Faculty details\n");
        printf("6. View All Users\n");
        printf("7. View All Courses\n");
        printf("8. Logout\n");
        printf("9. Exit\n");
        
        printf("\nEnter your choice: ");
        
        int choice;
        scanf("%d", &choice);
        getchar(); // Clear input buffer
        
        char request[BUFFER_SIZE];
        char response[BUFFER_SIZE];
        
        switch (choice) {
            case 1: { // Add Student
                clearScreen();
                displayTitle("Add New Student");
                
                char username[MAX_USERNAME_LENGTH];
                char password[MAX_PASSWORD_LENGTH];
                
                printf("Enter username: ");
                scanf("%s", username);
                getchar(); // Clear input buffer
                
                printf("Enter password: ");
                getPasswordInput(password);
                
                snprintf(request, BUFFER_SIZE, "ADMIN %d ADD_STUDENT %s %s", current_user_id, username, password);
                sendRequest(request, response);
                
                displaySuccess(response);
                break;
            }
            case 2: { // Add Faculty
                clearScreen();
                displayTitle("Add New Faculty");
                
                char username[MAX_USERNAME_LENGTH];
                char password[MAX_PASSWORD_LENGTH];
                
                printf("Enter username: ");
                scanf("%s", username);
                getchar(); // Clear input buffer
                
                printf("Enter password: ");
                getPasswordInput(password);
                
                snprintf(request, BUFFER_SIZE, "ADMIN %d ADD_FACULTY %s %s", current_user_id, username, password);
                sendRequest(request, response);
                
                displaySuccess(response);
                break;
            }
            case 3:{ // Add admin
            	clearScreen();
                displayTitle("Add New Admin");
                
                char username[MAX_USERNAME_LENGTH];
                char password[MAX_PASSWORD_LENGTH];
                
                printf("Enter username: ");
                scanf("%s", username);
                getchar(); // Clear input buffer
                
                printf("Enter password: ");
                getPasswordInput(password);
                
                snprintf(request, BUFFER_SIZE, "ADMIN %d ADD_ADMIN %s %s", current_user_id, username, password);
                sendRequest(request, response);
                
                displaySuccess(response);
                break;
            }
            case 4: { // Activate/Deactivate Student
                clearScreen();
                displayTitle("Activate/Deactivate Student");
                
                int studentId;
                
                printf("Enter student ID: ");
                scanf("%d", &studentId);
                getchar(); // Clear input buffer
                
                snprintf(request, BUFFER_SIZE, "ADMIN %d TOGGLE_STUDENT %d", current_user_id, studentId);
                sendRequest(request, response);
                
                displaySuccess(response);
                break;
            }
            case 5: { // Update User details
                clearScreen();
                displayTitle("Update User Details");
                
                int userId;
                char field[20];
                char value[MAX_PASSWORD_LENGTH];
                
                printf("Enter user ID: ");
                scanf("%d", &userId);
                getchar(); // Clear input buffer
                
                printf("What to update (username/password): ");
                scanf("%s", field);
                getchar(); // Clear input buffer
                
                if (strcmp(field, "password") == 0) {
                    printf("Enter new password: ");
                    getPasswordInput(value);
                } else {
                    printf("Enter new value: ");
                    scanf("%s", value);
                    getchar(); // Clear input buffer
                }
                
                snprintf(request, BUFFER_SIZE, "ADMIN %d UPDATE_USER %d %s %s", current_user_id, userId, field, value);
                sendRequest(request, response);
                
                displaySuccess(response);
                break;
            }
            case 6: { // View All Users
                clearScreen();
                displayTitle("All Users");
                
                snprintf(request, BUFFER_SIZE, "ADMIN %d VIEW_USERS", current_user_id);
                sendRequest(request, response);
                
                printf("%s\n", response);
                waitForEnter();
                break;
            }
            case 7: { // View All Courses
                clearScreen();
                displayTitle("All Courses");
                
                snprintf(request, BUFFER_SIZE, "ADMIN %d VIEW_COURSES", current_user_id);
                sendRequest(request, response);
                
                printf("%s\n", response);
                waitForEnter();
                break;
            }
            case 8: { // Logout
                is_logged_in = false;
                strcpy(current_user_type, "");
                current_user_id = -1;
                printf("Logged out successfully.\n");
                waitForEnter();
                return; // Return to login menu
            }
            case 9: { // Exit
                sendRequest("EXIT", response);
                cleanExit(0);
                break;
            }
            default:
                displayError("Invalid choice!");
        }
    }
}

// Student menu
void studentMenu() {
    while (is_logged_in) {
        clearScreen();
        displayTitle("Student Dashboard");
        
        printf("1. Enroll to new Courses\n");
        printf("2. Unenroll from already enrolled Courses\n");
        printf("3. View enrolled Courses\n");
        printf("4. View all available Courses\n");
        printf("5. Change Password\n");
        printf("6. Logout\n");
        printf("7. Exit\n");
        
        printf("\nEnter your choice: ");
        
        int choice;
        scanf("%d", &choice);
        getchar(); // Clear input buffer
        
        char request[BUFFER_SIZE];
        char response[BUFFER_SIZE];
        
        switch (choice) {
            case 1: { // Enroll to new course
                clearScreen();
                displayTitle("Enroll to New Course");
                
                // First, show available courses
                snprintf(request, BUFFER_SIZE, "STUDENT %d VIEW_COURSES", current_user_id);
                sendRequest(request, response);
                printf("%s\n", response);
                
                char courseCode[20];
                printf("\nEnter course code to enroll: ");
                scanf("%s", courseCode);
                getchar(); // Clear input buffer
                
                snprintf(request, BUFFER_SIZE, "STUDENT %d ENROLL %s", current_user_id, courseCode);
                sendRequest(request, response);
                
                displaySuccess(response);
                break;
            }
            case 2: { // Unenroll from course
                clearScreen();
                displayTitle("Unenroll from Course");
                
                // First, show enrolled courses
                snprintf(request, BUFFER_SIZE, "STUDENT %d VIEW_ENROLLED", current_user_id);
                sendRequest(request, response);
                printf("%s\n", response);
                
                char courseCode[20];
                printf("\nEnter course code to unenroll: ");
                scanf("%s", courseCode);
                getchar(); // Clear input buffer
                
                snprintf(request, BUFFER_SIZE, "STUDENT %d UNENROLL %s", current_user_id, courseCode);
                sendRequest(request, response);
                
                displaySuccess(response);
                break;
            }
            case 3: { // View enrolled courses
                clearScreen();
                displayTitle("Your Enrolled Courses");
                
                snprintf(request, BUFFER_SIZE, "STUDENT %d VIEW_ENROLLED", current_user_id);
                sendRequest(request, response);
                
                printf("%s\n", response);
                waitForEnter();
                break;
            }
            case 4: { // View all available courses
                clearScreen();
                displayTitle("All Available Courses");
                
                snprintf(request, BUFFER_SIZE, "STUDENT %d VIEW_COURSES", current_user_id);
                sendRequest(request, response);
                
                printf("%s\n", response);
                waitForEnter();
                break;
            }
            case 5: { // Change password
                clearScreen();
                displayTitle("Change Password");
                
                char oldPassword[MAX_PASSWORD_LENGTH];
                char newPassword[MAX_PASSWORD_LENGTH];
                
                printf("Enter current password: ");
                getPasswordInput(oldPassword);
                
                printf("\nEnter new password: ");
                getPasswordInput(newPassword);
                
                snprintf(request, BUFFER_SIZE, "STUDENT %d CHANGE_PASSWORD %s %s", current_user_id, oldPassword, newPassword);
                sendRequest(request, response);
                
                displaySuccess(response);
                break;
            }
            case 6: { // Logout
                is_logged_in = false;
                strcpy(current_user_type, "");
                current_user_id = -1;
                printf("Logged out successfully.\n");
                waitForEnter();
                return; // Return to login menu
            }
            case 7: { // Exit
                sendRequest("EXIT", response);
                cleanExit(0);
                break;
            }
            default:
                displayError("Invalid choice!");
        }
    }
}

// Faculty menu
void facultyMenu() {
    while (is_logged_in) {
        clearScreen();
        displayTitle("Faculty Dashboard");
        
        printf("1. Add new Course\n");
        printf("2. Remove offered Course\n");
        printf("3. View enrollments in Courses\n");
        printf("4. View your Courses\n");
        printf("5. Change Password\n");
        printf("6. Logout\n");
        printf("7. Exit\n");
        
        printf("\nEnter your choice: ");
        
        int choice;
        scanf("%d", &choice);
        getchar(); // Clear input buffer
        
        char request[BUFFER_SIZE];
        char response[BUFFER_SIZE];
        
        switch (choice) {
            case 1: { // Add new course
                clearScreen();
                displayTitle("Add New Course");
                
                char courseCode[20];
                char courseName[MAX_COURSE_NAME_LENGTH];
                int seats;
                
                printf("Enter course code: ");
                scanf("%s", courseCode);
                getchar(); // Clear input buffer
                
                printf("Enter total seats: ");
                scanf("%d", &seats);
                getchar(); // Clear input buffer
                
                printf("Enter course name: ");
                fgets(courseName, MAX_COURSE_NAME_LENGTH, stdin);
                // Remove trailing newline
                size_t len = strlen(courseName);
                if (len > 0 && courseName[len-1] == '\n') {
                    courseName[len-1] = '\0';
                }
                
                snprintf(request, BUFFER_SIZE, "FACULTY %d ADD_COURSE %s %d %s", current_user_id, courseCode, seats, courseName);
                sendRequest(request, response);
                
                displaySuccess(response);
                break;
            }
            case 2: { // Remove offered course
                clearScreen();
                displayTitle("Remove Course");
                
                // First, show faculty's courses
                snprintf(request, BUFFER_SIZE, "FACULTY %d VIEW_COURSES", current_user_id);
                sendRequest(request, response);
                printf("%s\n", response);
                
                char courseCode[20];
                printf("\nEnter course code to remove: ");
                scanf("%s", courseCode);
                getchar(); // Clear input buffer
                
                snprintf(request, BUFFER_SIZE, "FACULTY %d REMOVE_COURSE %s", current_user_id, courseCode);
                sendRequest(request, response);
                
                displaySuccess(response);
                break;
            }
            case 3: { // View enrollments in courses
                clearScreen();
                displayTitle("Course Enrollments");
                
                snprintf(request, BUFFER_SIZE, "FACULTY %d VIEW_ENROLLMENTS", current_user_id);
                sendRequest(request, response);
                
                printf("%s\n", response);
                waitForEnter();
                break;
            }
            case 4: { // View faculty's courses
                clearScreen();
                displayTitle("Your Courses");
                
                snprintf(request, BUFFER_SIZE, "FACULTY %d VIEW_COURSES", current_user_id);
                sendRequest(request, response);
                
                printf("%s\n", response);
                waitForEnter();
                break;
            }
            case 5: { // Change password
                clearScreen();
                displayTitle("Change Password");
                
                char oldPassword[MAX_PASSWORD_LENGTH];
                char newPassword[MAX_PASSWORD_LENGTH];
                
                printf("Enter current password: ");
                getPasswordInput(oldPassword);
                
                printf("\nEnter new password: ");
                getPasswordInput(newPassword);
                
                snprintf(request, BUFFER_SIZE, "FACULTY %d CHANGE_PASSWORD %s %s", current_user_id, oldPassword, newPassword);
                sendRequest(request, response);
                
                displaySuccess(response);
                break;
            }
            case 6: { // Logout
                is_logged_in = false;
                strcpy(current_user_type, "");
                current_user_id = -1;
                printf("Logged out successfully.\n");
                waitForEnter();
                return; // Return to login menu
            }
            case 7: { // Exit
                sendRequest("EXIT", response);
                cleanExit(0);
                break;
            }
            default:
                displayError("Invalid choice!");
        }
    }
}

int main() {
    // Set up signal handler
    signal(SIGINT, cleanExit);
    
    // Connect to server
    connectToServer();
    
    // Start with login menu
    while (true) {
        loginMenu();
    }
    
    return 0;
}
