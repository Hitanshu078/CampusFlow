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
