// Academia Portal Server
// CS-513 System Software Mini Project

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <semaphore.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define MAX_COURSE_SEATS 10
#define MAX_USERS 1000
#define MAX_COURSES 1000
#define MAX_ENROLLMENTS 10000
#define MAX_STR 256

// Semaphore for controlling access to critical sections
sem_t mutex;

// User types
enum UserType {
    ADMIN = 1,
    STUDENT = 2,
    FACULTY = 3
};

// Structure to hold user information
typedef struct {
    int id;
    char username[MAX_STR];
    char password[MAX_STR];
    enum UserType type;
    int active; // 1 for true, 0 for false
} User;

// Structure to hold course information
typedef struct {
    int id;
    char code[MAX_STR];
    char name[MAX_STR];
    int facultyId;
    int totalSeats;
    int enrolledStudents;
} Course;

// Structure to hold enrollment information
typedef struct {
    int studentId;
    int courseId;
} Enrollment;

// Global variables for storing data
User* users = NULL;
int users_size = 0;
Course* courses = NULL;
int courses_size = 0;
Enrollment* enrollments = NULL;
int enrollments_size = 0;

// File paths
const char* USER_FILE = "users.txt";
const char* COURSE_FILE = "courses.txt";
const char* ENROLLMENT_FILE = "enrollments.txt";

// Function prototypes
void loadData();
void saveData();
void* handleClient(void* client_socket);
char* processRequest(const char* request, int clientSocket);
char* loginUser(const char* username, const char* password);
char* handleAdminRequest(const char* request, int userId);
char* handleStudentRequest(const char* request, int userId);
char* handleFacultyRequest(const char* request, int userId);
User* findUserById(int id);
User* findUserByUsername(const char* username);
Course* findCourseById(int id);
Course* findCourseByCode(const char* code);
int isEnrolled(int studentId, int courseId);
void acquireReadLock(const char* filename);
void acquireWriteLock(const char* filename);
void releaseLock(const char* filename);
char* strdup(const char* s); // For systems lacking strdup


// Signal handler for cleaning up when server is closed
void signalHandler(int signal_num) {
    printf("\nSignal %d received. Cleaning up and exiting...\n", signal_num);
    saveData();
    sem_destroy(&mutex);
    free(users);
    free(courses);
    free(enrollments);
    exit(signal_num);
}

int main() {
    // Initialize semaphore
    sem_init(&mutex, 0, 1);
    
    // Set up signal handler
    signal(SIGINT, signalHandler);
    
    // Load data from files
    loadData();
    
    // Create a socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options to reuse address and port
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Server address configuration
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind the socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Start listening for connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Academia Portal Server started on port %d\n", PORT);
    printf("Waiting for connections...\n");
    
    // Accept and handle client connections
    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_addrlen = sizeof(client_address);
        
        // Accept a new client connection
        int* client_sock = (int*)malloc(sizeof(int));
        *client_sock = accept(server_fd, (struct sockaddr *)&client_address, &client_addrlen);
        
        if (*client_sock < 0) {
            perror("Accept failed");
            free(client_sock);
            continue;
        }
        
        // Get client IP address
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_address.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("New connection from %s:%d\n", client_ip, ntohs(client_address.sin_port));
        
        // Create a new thread to handle the client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handleClient, (void*)client_sock) != 0) {
            perror("Thread creation failed");
            close(*client_sock);
            free(client_sock);
            continue;
        }
        
        // Detach the thread to allow it to run independently
        pthread_detach(thread_id);
    }
    
    // Clean up (unreachable due to signal handler)
    close(server_fd);
    sem_destroy(&mutex);
    free(users);
    free(courses);
    free(enrollments);
    
    return 0;
}

// Load all data from files
void loadData() {
    FILE* file;
    char line[1024];
    
    // Load users
    file = fopen(USER_FILE, "r");
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            User user;
            char userType[20];
            int active;
            
            // Parse user data
            sscanf(line, "%d %s %s %s %d", &user.id, user.username, user.password, userType, &active);
            
            // Convert user type to enum
            if (strcmp(userType, "ADMIN") == 0) user.type = ADMIN;
            else if (strcmp(userType, "STUDENT") == 0) user.type = STUDENT;
            else if (strcmp(userType, "FACULTY") == 0) user.type = FACULTY;
            
            user.active = active;
            
            // Add to users array
            users = (User*)realloc(users, (users_size + 1) * sizeof(User));
            users[users_size++] = user;
        }
        fclose(file);
    } else {
        // Create default admin account if file doesn't exist
        User admin;
        admin.id = 1;
        strcpy(admin.username, "admin");
        strcpy(admin.password, "admin123");
        admin.type = ADMIN;
        admin.active = 1;
        users = (User*)realloc(users, (users_size + 1) * sizeof(User));
        users[users_size++] = admin;

        file = fopen(USER_FILE, "w");
        fprintf(file, "%d %s %s ADMIN %d\n", admin.id, admin.username, admin.password, admin.active);
        fclose(file);
    }

    // Load courses
    file = fopen(COURSE_FILE, "r");
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            Course course;
            char temp[1024];
            // Parse course data
            if (sscanf(line, "%d %s %d %d %d %[^\n]", &course.id, course.code, &course.facultyId, 
                      &course.totalSeats, &course.enrolledStudents, temp) >= 5) {
                strcpy(course.name, temp);
                courses = (Course*)realloc(courses, (courses_size + 1) * sizeof(Course));
                courses[courses_size++] = course;
            }
        }
        fclose(file);
    }

    // Load enrollments
    file = fopen(ENROLLMENT_FILE, "r");
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            Enrollment enrollment;
            sscanf(line, "%d %d", &enrollment.studentId, &enrollment.courseId);
            enrollments = (Enrollment*)realloc(enrollments, (enrollments_size + 1) * sizeof(Enrollment));
            enrollments[enrollments_size++] = enrollment;
        }
        fclose(file);
    }
}

// Save all data to files
void saveData() {
    sem_wait(&mutex);

    // Save users
    FILE* userFile = fopen(USER_FILE, "w");
    for (int i = 0; i < users_size; i++) {
        const char* userType = users[i].type == ADMIN ? "ADMIN" : 
                             users[i].type == STUDENT ? "STUDENT" : "FACULTY";
        fprintf(userFile, "%d %s %s %s %d\n", users[i].id, users[i].username, 
                users[i].password, userType, users[i].active);
    }
    fclose(userFile);

    // Save courses
    FILE* courseFile = fopen(COURSE_FILE, "w");
    for (int i = 0; i < courses_size; i++) {
        fprintf(courseFile, "%d %s %d %d %d %s\n", courses[i].id, courses[i].code, 
                courses[i].facultyId, courses[i].totalSeats, courses[i].enrolledStudents, 
                courses[i].name);
    }
    fclose(courseFile);

    // Save enrollments
    FILE* enrollmentFile = fopen(ENROLLMENT_FILE, "w");
    for (int i = 0; i < enrollments_size; i++) {
        fprintf(enrollmentFile, "%d %d\n", enrollments[i].studentId, enrollments[i].courseId);
    }
    fclose(enrollmentFile);

    sem_post(&mutex);
}

