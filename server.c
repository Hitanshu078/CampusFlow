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
