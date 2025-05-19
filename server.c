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

// Function to handle client connections
void* handleClient(void* client_socket) {
    int sock = *(int*)client_socket;
    free(client_socket);

    char buffer[BUFFER_SIZE] = {0};

    while (1) {
        // Clear the buffer
        memset(buffer, 0, BUFFER_SIZE);

        // Read client message
        ssize_t bytesRead = read(sock, buffer, BUFFER_SIZE - 1);
        if (bytesRead <= 0) {
            close(sock);
            printf("Client disconnected\n");
            return NULL;
        }

        // Process the request
        char* response = processRequest(buffer, sock);

        // Send response back to client
        send(sock, response, strlen(response), 0);
        free(response);

        // If client sent "EXIT", close the connection
        if (strcmp(buffer, "EXIT") == 0) {
            printf("Client requested to exit\n");
            close(sock);
            return NULL;
        }
    }

    return NULL;
}

// Process client requests
char* processRequest(const char* request, int clientSocket) {
    char* response = (char*)malloc(BUFFER_SIZE);
    response[0] = '\0';

    char* token = strtok((char*)request, " ");
    if (!token) {
        strcpy(response, "Invalid request");
        return response;
    }

    char command[50];
    strcpy(command, token);

    // Handle login request
    if (strcmp(command, "LOGIN") == 0) {
        char* username = strtok(NULL, " ");
        char* password = strtok(NULL, " ");
        if (username && password) {
            strcpy(response, loginUser(username, password));
        } else {
            strcpy(response, "Invalid login format");
        }
    }
    // Handle role-specific requests
    else if (strcmp(command, "ADMIN") == 0 || strcmp(command, "STUDENT") == 0 || 
             strcmp(command, "FACULTY") == 0) {
        char* userIdStr = strtok(NULL, " ");
        if (!userIdStr) {
            strcpy(response, "Invalid request format");
            return response;
        }
        int userId = atoi(userIdStr);
        char* subRequest = strtok(NULL, "");
        if (!subRequest) subRequest = "";

        if (strcmp(command, "ADMIN") == 0) {
            strcpy(response, handleAdminRequest(subRequest, userId));
        } else if (strcmp(command, "STUDENT") == 0) {
            strcpy(response, handleStudentRequest(subRequest, userId));
        } else {
            strcpy(response, handleFacultyRequest(subRequest, userId));
        }
    } else {
        strcpy(response, "Invalid request format");
    }

    return response;
}

// User login
char* loginUser(const char* username, const char* password) {
    char* response = (char*)malloc(BUFFER_SIZE);
    for (int i = 0; i < users_size; i++) {
        if (strcmp(users[i].username, username) == 0 && 
            strcmp(users[i].password, password) == 0) {
            if (!users[i].active) {
                strcpy(response, "LOGIN_FAILED Account deactivated");
                return response;
            }
            const char* userType = users[i].type == ADMIN ? "ADMIN" : 
                                 users[i].type == STUDENT ? "STUDENT" : "FACULTY";
            sprintf(response, "LOGIN_SUCCESS %s %d", userType, users[i].id);
            return response;
        }
    }
    strcpy(response, "LOGIN_FAILED Invalid credentials");
    return response;
}

// Handle admin requests
char* handleAdminRequest(const char* request, int userId) {
    char* response = (char*)malloc(BUFFER_SIZE);
    User* admin = findUserById(userId);
    if (!admin || admin->type != ADMIN) {
        strcpy(response, "Access denied");
        return response;
    }

    char* token = strtok((char*)request, " ");
    if (!token) {
        strcpy(response, "Invalid admin request");
        return response;
    }

    char command[50];
    strcpy(command, token);

    if (strcmp(command, "ADD_STUDENT") == 0) {
        char* username = strtok(NULL, " ");
        char* password = strtok(NULL, " ");
        if (!username || !password) {
            strcpy(response, "Invalid format");
            return response;
        }
        if (findUserByUsername(username)) {
            sprintf(response, "Student with username %s already exists", username);
            return response;
        }
        User student;
        student.id = users_size ? users[users_size-1].id + 1 : 1;
        strncpy(student.username, username, MAX_STR-1);
        student.username[MAX_STR-1] = '\0';
        strncpy(student.password, password, MAX_STR-1);
        student.password[MAX_STR-1] = '\0';
        student.type = STUDENT;
        student.active = 1;
        users = (User*)realloc(users, (users_size + 1) * sizeof(User));
        users[users_size++] = student;
        saveData();
        sprintf(response, "Student added successfully with ID %d", student.id);
    }
    else if (strcmp(command, "ADD_FACULTY") == 0) {
        char* username = strtok(NULL, " ");
        char* password = strtok(NULL, " ");
        if (!username || !password) {
            strcpy(response, "Invalid format");
            return response;
        }
        if (findUserByUsername(username)) {
            sprintf(response, "Faculty with username %s already exists", username);
            return response;
        }
        User faculty;
        faculty.id = users_size ? users[users_size-1].id + 1 : 1;
        strncpy(faculty.username, username, MAX_STR-1);
        faculty.username[MAX_STR-1] = '\0';
        strncpy(faculty.password, password, MAX_STR-1);
        faculty.password[MAX_STR-1] = '\0';
        faculty.type = FACULTY;
        faculty.active = 1;
        users = (User*)realloc(users, (users_size + 1) * sizeof(User));
        users[users_size++] = faculty;
        saveData();
        sprintf(response, "Faculty added successfully with ID %d", faculty.id);
    }
    else if(strcmp(command,"ADD_ADMIN")==0){
    	char* username = strtok(NULL, " ");
        char* password = strtok(NULL, " ");
        if (!username || !password) {
            strcpy(response, "Invalid format");
            return response;
        }
        if (findUserByUsername(username)) {
            sprintf(response, "Admin with username %s already exists", username);
            return response;
        }
        User admin;
        admin.id = users_size ? users[users_size-1].id + 1 : 1;
        strncpy(admin.username, username, MAX_STR-1);
        admin.username[MAX_STR-1] = '\0';
        strncpy(admin.password, password, MAX_STR-1);
        admin.password[MAX_STR-1] = '\0';
        admin.type = ADMIN;
        admin.active = 1;
        users = (User*)realloc(users, (users_size + 1) * sizeof(User));
        users[users_size++] = admin;
        saveData();
        sprintf(response, "Admin added successfully with ID %d", admin.id);
    }
    	
    else if (strcmp(command, "TOGGLE_STUDENT") == 0) {
        char* studentIdStr = strtok(NULL, " ");
        if (!studentIdStr) {
            strcpy(response, "Invalid format");
            return response;
        }
        int studentId = atoi(studentIdStr);
        User* student = findUserById(studentId);
        if (!student || student->type != STUDENT) {
            strcpy(response, "Student not found");
            return response;
        }
        student->active = !student->active;
        saveData();
        sprintf(response, "Student %s %s successfully", student->username, 
                student->active ? "activated" : "deactivated");
    }
    else if (strcmp(command, "UPDATE_USER") == 0) {
        char* userIdStr = strtok(NULL, " ");
        char* field = strtok(NULL, " ");
        char* value = strtok(NULL, " ");
        if (!userIdStr || !field || !value) {
            strcpy(response, "Invalid format");
            return response;
        }
        int userId = atoi(userIdStr);
        User* user = findUserById(userId);
        if (!user) {
            strcpy(response, "User not found");
            return response;
        }
        if (strcmp(field, "password") == 0) {
            strncpy(user->password, value, MAX_STR-1);
            user->password[MAX_STR-1] = '\0';
            saveData();
            strcpy(response, "Password updated successfully");
        }
        else if (strcmp(field, "username") == 0) {
            if (findUserByUsername(value)) {
                sprintf(response, "Username %s already exists", value);
            } else {
                strncpy(user->username, value, MAX_STR-1);
                user->username[MAX_STR-1] = '\0';
                saveData();
                strcpy(response, "Username updated successfully");
            }
        }
        else {
            strcpy(response, "Invalid field to update");
        }
    }
    else if (strcmp(command, "VIEW_USERS") == 0) {
        char* result = (char*)malloc(BUFFER_SIZE);
        strcpy(result, "Users list:\n");
        for (int i = 0; i < users_size; i++) {
            const char* userType = users[i].type == ADMIN ? "ADMIN" : 
                                 users[i].type == STUDENT ? "STUDENT" : "FACULTY";
            char line[256];
            sprintf(line, "ID: %d, Username: %s, Type: %s, Status: %s\n", 
                    users[i].id, users[i].username, userType, 
                    users[i].active ? "Active" : "Inactive");
            strncat(result, line, BUFFER_SIZE - strlen(result) - 1);
        }
        strcpy(response, result);
        free(result);
    }
    else if (strcmp(command, "VIEW_COURSES") == 0) {
        acquireReadLock(COURSE_FILE);
        char* result = (char*)malloc(BUFFER_SIZE);
        strcpy(result, "Courses list:\n");
        for (int i = 0; i < courses_size; i++) {
            User* faculty = findUserById(courses[i].facultyId);
            char line[256];
            sprintf(line, "ID: %d, Code: %s, Name: %s, Faculty: %s, Seats: %d/%d\n", 
                    courses[i].id, courses[i].code, courses[i].name, 
                    faculty ? faculty->username : "Unknown", 
                    courses[i].enrolledStudents, courses[i].totalSeats);
            strncat(result, line, BUFFER_SIZE - strlen(result) - 1);
        }
        releaseLock(COURSE_FILE);
        strcpy(response, result);
        free(result);
    }
    else {
        strcpy(response, "Invalid admin command");
    }

    return response;
}

// Handle student requests
char* handleStudentRequest(const char* request, int userId) {
    char* response = (char*)malloc(BUFFER_SIZE);
    User* student = findUserById(userId);
    if (!student || student->type != STUDENT) {
        strcpy(response, "Access denied");
        return response;
    }

    char* token = strtok((char*)request, " ");
    if (!token) {
        strcpy(response, "Invalid student request");
        return response;
    }

    char command[50];
    strcpy(command, token);

    if (strcmp(command, "ENROLL") == 0) {
        char* courseCode = strtok(NULL, " ");
        if (!courseCode) {
            strcpy(response, "Invalid format");
            return response;
        }
        acquireWriteLock(COURSE_FILE);
        acquireWriteLock(ENROLLMENT_FILE);
        Course* course = findCourseByCode(courseCode);
        if (!course) {
            releaseLock(COURSE_FILE);
            releaseLock(ENROLLMENT_FILE);
            strcpy(response, "Course not found");
            return response;
        }
        if (isEnrolled(student->id, course->id)) {
            releaseLock(COURSE_FILE);
            releaseLock(ENROLLMENT_FILE);
            strcpy(response, "Already enrolled in this course");
            return response;
        }
        if (course->enrolledStudents >= course->totalSeats) {
            releaseLock(COURSE_FILE);
            releaseLock(ENROLLMENT_FILE);
            strcpy(response, "Course is full");
            return response;
        }
        Enrollment enrollment;
        enrollment.studentId = student->id;
        enrollment.courseId = course->id;
        enrollments = (Enrollment*)realloc(enrollments, (enrollments_size + 1) * sizeof(Enrollment));
        enrollments[enrollments_size++] = enrollment;
        course->enrolledStudents++;
        saveData();
        releaseLock(COURSE_FILE);
        releaseLock(ENROLLMENT_FILE);
        sprintf(response, "Successfully enrolled in %s - %s", course->code, course->name);
    }
    else if (strcmp(command, "UNENROLL") == 0) {
        char* courseCode = strtok(NULL, " ");
        if (!courseCode) {
            strcpy(response, "Invalid format");
            return response;
        }
        acquireWriteLock(COURSE_FILE);
        acquireWriteLock(ENROLLMENT_FILE);
        Course* course = findCourseByCode(courseCode);
        if (!course) {
            releaseLock(COURSE_FILE);
            releaseLock(ENROLLMENT_FILE);
            strcpy(response, "Course not found");
            return response;
        }
        int found = 0;
        for (int i = 0; i < enrollments_size; i++) {
            if (enrollments[i].studentId == student->id && enrollments[i].courseId == course->id) {
                for (int j = i; j < enrollments_size-1; j++) {
                    enrollments[j] = enrollments[j+1];
                }
                enrollments_size--;
                enrollments = (Enrollment*)realloc(enrollments, enrollments_size * sizeof(Enrollment));
                found = 1;
                break;
            }
        }
        if (!found) {
            releaseLock(COURSE_FILE);
            releaseLock(ENROLLMENT_FILE);
            strcpy(response, "Not enrolled in this course");
            return response;
        }
        course->enrolledStudents--;
        saveData();
        releaseLock(COURSE_FILE);
        releaseLock(ENROLLMENT_FILE);
        sprintf(response, "Successfully unenrolled from %s - %s", course->code, course->name);
    }
    else if (strcmp(command, "VIEW_ENROLLED") == 0) {
        acquireReadLock(COURSE_FILE);
        acquireReadLock(ENROLLMENT_FILE);
        char* result = (char*)malloc(BUFFER_SIZE);
        strcpy(result, "Enrolled courses:\n");
        int hasEnrollments = 0;
        for (int i = 0; i < enrollments_size; i++) {
            if (enrollments[i].studentId == student->id) {
                Course* course = findCourseById(enrollments[i].courseId);
                if (course) {
                    char line[256];
                    sprintf(line, "Code: %s, Name: %s\n", course->code, course->name);
                    strncat(result, line, BUFFER_SIZE - strlen(result) - 1);
                    hasEnrollments = 1;
                }
            }
        }
        releaseLock(COURSE_FILE);
        releaseLock(ENROLLMENT_FILE);
        if (!hasEnrollments) {
            strcpy(response, "You are not enrolled in any courses");
        } else {
            strcpy(response, result);
        }
        free(result);
    }
    else if (strcmp(command, "VIEW_COURSES") == 0) {
        acquireReadLock(COURSE_FILE);
        char* result = (char*)malloc(BUFFER_SIZE);
        strcpy(result, "Available courses:\n");
        for (int i = 0; i < courses_size; i++) {
            User* faculty = findUserById(courses[i].facultyId);
            char line[256];
            sprintf(line, "Code: %s, Name: %s, Faculty: %s, Available seats: %d/%d\n", 
                    courses[i].code, courses[i].name, 
                    faculty ? faculty->username : "Unknown", 
                    courses[i].totalSeats - courses[i].enrolledStudents, 
                    courses[i].totalSeats);
            strncat(result, line, BUFFER_SIZE - strlen(result) - 1);
        }
        releaseLock(COURSE_FILE);
        strcpy(response, result);
        free(result);
    }
    else if (strcmp(command, "CHANGE_PASSWORD") == 0) {
        char* oldPassword = strtok(NULL, " ");
        char* newPassword = strtok(NULL, " ");
        if (!oldPassword || !newPassword) {
            strcpy(response, "Invalid format");
            return response;
        }
        if (strcmp(student->password, oldPassword) != 0) {
            strcpy(response, "Incorrect current password");
            return response;
        }
        strncpy(student->password, newPassword, MAX_STR-1);
        student->password[MAX_STR-1] = '\0';
        saveData();
        strcpy(response, "Password changed successfully");
    }
    else {
        strcpy(response, "Invalid student command");
    }

    return response;
}

// Handle faculty requests
char* handleFacultyRequest(const char* request, int userId) {
    char* response = (char*)malloc(BUFFER_SIZE);
    User* faculty = findUserById(userId);
    if (!faculty || faculty->type != FACULTY) {
        strcpy(response, "Access denied");
        return response;
    }

    char* token = strtok((char*)request, " ");
    if (!token) {
        strcpy(response, "Invalid faculty request");
        return response;
    }

    char command[50];
    strcpy(command, token);

    if (strcmp(command, "ADD_COURSE") == 0) {
        char* courseCode = strtok(NULL, " ");
        char* seatsStr = strtok(NULL, " ");
        char* courseName = strtok(NULL, "");
        if (!courseCode || !seatsStr || !courseName) {
            strcpy(response, "Invalid format");
            return response;
        }
        int seats = atoi(seatsStr);
        acquireWriteLock(COURSE_FILE);
        for (int i = 0; i < courses_size; i++) {
            if (strcmp(courses[i].code, courseCode) == 0) {
                releaseLock(COURSE_FILE);
                sprintf(response, "Course with code %s already exists", courseCode);
                return response;
            }
        }
        Course course;
        course.id = courses_size ? courses[courses_size-1].id + 1 : 1;
        strncpy(course.code, courseCode, MAX_STR-1);
        course.code[MAX_STR-1] = '\0';
        strncpy(course.name, courseName, MAX_STR-1);
        course.name[MAX_STR-1] = '\0';
        course.facultyId = faculty->id;
        course.totalSeats = seats;
        course.enrolledStudents = 0;
        courses = (Course*)realloc(courses, (courses_size + 1) * sizeof(Course));
        courses[courses_size++] = course;
        saveData();
        releaseLock(COURSE_FILE);
        sprintf(response, "Course added successfully: %s - %s", courseCode, courseName);
    }
    else if (strcmp(command, "REMOVE_COURSE") == 0) {
        char* courseCode = strtok(NULL, " ");
        if (!courseCode) {
            strcpy(response, "Invalid format");
            return response;
        }
        acquireWriteLock(COURSE_FILE);
        acquireWriteLock(ENROLLMENT_FILE);
        int courseId = -1;
        for (int i = 0; i < courses_size; i++) {
            if (strcmp(courses[i].code, courseCode) == 0 && courses[i].facultyId == faculty->id) {
                courseId = courses[i].id;
                for (int j = i; j < courses_size-1; j++) {
                    courses[j] = courses[j+1];
                }
                courses_size--;
                courses = (Course*)realloc(courses, courses_size * sizeof(Course));
                break;
            }
        }
        if (courseId == -1) {
            releaseLock(COURSE_FILE);
            releaseLock(ENROLLMENT_FILE);
            strcpy(response, "Course not found or you don't have permission to remove it");
            return response;
        }
        int new_size = 0;
        for (int i = 0; i < enrollments_size; i++) {
            if (enrollments[i].courseId != courseId) {
                enrollments[new_size] = enrollments[i];
                new_size++;
            }
        }
        enrollments_size = new_size;
        enrollments = (Enrollment*)realloc(enrollments, enrollments_size * sizeof(Enrollment));
        saveData();
        releaseLock(COURSE_FILE);
        releaseLock(ENROLLMENT_FILE);
        sprintf(response, "Course %s removed successfully", courseCode);
    }
    else if (strcmp(command, "VIEW_ENROLLMENTS") == 0) {
        acquireReadLock(COURSE_FILE);
        acquireReadLock(ENROLLMENT_FILE);
        char* result = (char*)malloc(BUFFER_SIZE);
        strcpy(result, "Course enrollments:\n");
        int hasCourses = 0;
        for (int i = 0; i < courses_size; i++) {
            if (courses[i].facultyId == faculty->id) {
                char line[256];
                sprintf(line, "\nCourse: %s - %s\nEnrolled students: %d/%d\n", 
                        courses[i].code, courses[i].name, 
                        courses[i].enrolledStudents, courses[i].totalSeats);
                strncat(result, line, BUFFER_SIZE - strlen(result) - 1);
                int hasStudents = 0;
                for (int j = 0; j < enrollments_size; j++) {
                    if (enrollments[j].courseId == courses[i].id) {
                        User* student = findUserById(enrollments[j].studentId);
                        if (student) {
                            sprintf(line, "- %s (ID: %d)\n", student->username, student->id);
                            strncat(result, line, BUFFER_SIZE - strlen(result) - 1);
                            hasStudents = 1;
                        }
                    }
                }
                if (!hasStudents) {
                    strncat(result, "- No students enrolled yet\n", 
                            BUFFER_SIZE - strlen(result) - 1);
                }
                hasCourses = 1;
            }
        }
        releaseLock(COURSE_FILE);
        releaseLock(ENROLLMENT_FILE);
        if (!hasCourses) {
            strcpy(response, "You have not offered any courses");
        } else {
            strcpy(response, result);
        }
        free(result);
    }
    else if (strcmp(command, "VIEW_COURSES") == 0) {
        acquireReadLock(COURSE_FILE);
        char* result = (char*)malloc(BUFFER_SIZE);
        strcpy(result, "Your courses:\n");
        int hasCourses = 0;
        for (int i = 0; i < courses_size; i++) {
            if (courses[i].facultyId == faculty->id) {
                char line[256];
                sprintf(line, "Code: %s, Name: %s, Enrollment: %d/%d\n", 
                        courses[i].code, courses[i].name, 
                        courses[i].enrolledStudents, courses[i].totalSeats);
                strncat(result, line, BUFFER_SIZE - strlen(result) - 1);
                hasCourses = 1;
            }
        }
        releaseLock(COURSE_FILE);
        if (!hasCourses) {
            strcpy(response, "You have not offered any courses");
        } else {
            strcpy(response, result);
        }
        free(result);
    }
    else if (strcmp(command, "CHANGE_PASSWORD") == 0) {
        char* oldPassword = strtok(NULL, " ");
        char* newPassword = strtok(NULL, " ");
        if (!oldPassword || !newPassword) {
            strcpy(response, "Invalid format");
            return response;
        }
        if (strcmp(faculty->password, oldPassword) != 0) {
            strcpy(response, "Incorrect current password");
            return response;
        }
        strncpy(faculty->password, newPassword, MAX_STR-1);
        faculty->password[MAX_STR-1] = '\0';
        saveData();
        strcpy(response, "Password changed successfully");
    }
    else {
        strcpy(response, "Invalid faculty command");
    }

    return response;
}

// Find a user by ID
User* findUserById(int id) {
    for (int i = 0; i < users_size; i++) {
        if (users[i].id == id) {
            return &users[i];
        }
    }
    return NULL;
}

// Find a user by username
User* findUserByUsername(const char* username) {
    for (int i = 0; i < users_size; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return &users[i];
        }
    }
    return NULL;
}

// Find a course by ID
Course* findCourseById(int id) {
    for (int i = 0; i < courses_size; i++) {
        if (courses[i].id == id) {
            return &courses[i];
        }
    }
    return NULL;
}

// Find a course by code
Course* findCourseByCode(const char* code) {
    for (int i = 0; i < courses_size; i++) {
        if (strcmp(courses[i].code, code) == 0) {
            return &courses[i];
        }
    }
    return NULL;
}

// Check if a student is enrolled in a course
int isEnrolled(int studentId, int courseId) {
    for (int i = 0; i < enrollments_size; i++) {
        if (enrollments[i].studentId == studentId && enrollments[i].courseId == courseId) {
            return 1;
        }
    }
    return 0;
}

// Acquire a read lock on a file
void acquireReadLock(const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd != -1) {
        flock(fd, LOCK_SH);
        close(fd);
    }
}

// Acquire a write lock on a file
void acquireWriteLock(const char* filename) {
    int fd = open(filename, O_WRONLY | O_CREAT, 0644);
    if (fd != -1) {
        flock(fd, LOCK_EX);
        close(fd);
    }
}

// Release a lock on a file
void releaseLock(const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd != -1) {
        flock(fd, LOCK_UN);
        close(fd);
    }
}

// strdup implementation for systems that lack it
char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* new = (char*)malloc(len);
    if (new) {
        memcpy(new, s, len);
    }
    return new;
}
