#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#define DEFAULT_ROOM_CAPACITY 30
#define EXAM_DURATION_SECONDS 3
#define MAX_STUDENTS 1000
#define AUTO_ID_BASE 10001
#define LOCK_TIMEOUT_SECONDS 2
#define NUM_EXAM_SLOTS 2
#define MAX_USERS 100
#define MAX_USERNAME_LEN 50
#define MAX_PASSWORD_LEN 50
#define ADMIN_USERNAME "admin"
#define ADMIN_PASSWORD "admin123"

typedef enum { STUDENT, ADMIN } UserRole;
typedef enum { PENDING, APPROVED, REJECTED } ApplicationStatus;

typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    UserRole role;
    bool has_applied;
    ApplicationStatus application_status;
} User;

typedef struct {
    int id;
    int room_number;
    pthread_t thread;
    bool entered;
    bool left;
    bool rejected;
    int exam_slot;
    char username[MAX_USERNAME_LEN];
} Student;

typedef struct {
    int room_number;
    int current_capacity;
    int max_capacity;
    pthread_mutex_t lock;
} Room;

// Function declarations
void run_simulation();
void view_student_status();
void student_menu();
void admin_menu();
void cleanup_resources();
void print_summary();
void print_welcome_message();
void reset_rooms_for_second_slot();
bool try_lock(pthread_mutex_t *mutex);
void* student_thread(void* arg);
void* exam_supervisor_thread(void* arg);
void view_all_applications();
void auto_approve_all_applications();
void add_student_manually();
void approve_application(char *username);
void reject_application(char *username);
void display_pending_applications();
void view_exam_status();
void apply_for_exam();
bool login();
bool register_user();
void initialize_default_admin();
void trim_newline(char *str);

sem_t exam_start_sem[NUM_EXAM_SLOTS];
sem_t exam_end_sem[NUM_EXAM_SLOTS];
pthread_mutex_t hall_lock;
pthread_mutex_t entry_lock[NUM_EXAM_SLOTS];
Room *rooms[NUM_EXAM_SLOTS] = {NULL, NULL};
Student *students = NULL;
User users[MAX_USERS];
int user_count = 0;
int total_students = 0;
int num_rooms = 0;
int room_capacity = DEFAULT_ROOM_CAPACITY;
User current_user;

void trim_newline(char *str) {
    int len = strlen(str);
    if (len > 0 && str[len-1] == '\n') {
        str[len-1] = '\0';
    }
}

void initialize_default_admin() {
    strcpy(users[0].username, ADMIN_USERNAME);
    strcpy(users[0].password, ADMIN_PASSWORD);
    users[0].role = ADMIN;
    users[0].has_applied = false;
    users[0].application_status = PENDING;
    user_count = 1;
}

bool register_user() {
    if (user_count >= MAX_USERS) {
        printf("Maximum user limit reached!\n");
        return false;
    }

    User new_user;
    printf("\n=== Registration ===\n");
    
    printf("Enter username: ");
    fgets(new_user.username, MAX_USERNAME_LEN, stdin);
    trim_newline(new_user.username);
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, new_user.username) == 0) {
            printf("Username already exists!\n");
            return false;
        }
    }
    
    printf("Enter password: ");
    fgets(new_user.password, MAX_PASSWORD_LEN, stdin);
    trim_newline(new_user.password);
    
    new_user.role = STUDENT;
    new_user.has_applied = false;
    new_user.application_status = PENDING;
    users[user_count++] = new_user;
    
    printf("Registration successful! Please login.\n");
    return true;
}

bool login() {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    
    printf("\n=== Login ===\n");
    printf("Username: ");
    fgets(username, MAX_USERNAME_LEN, stdin);
    trim_newline(username);
    
    printf("Password: ");
    fgets(password, MAX_PASSWORD_LEN, stdin);
    trim_newline(password);
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 && 
            strcmp(users[i].password, password) == 0) {
            current_user = users[i];
            return true;
        }
    }
    
    printf("Invalid username or password!\n");
    return false;
}

void apply_for_exam() {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, current_user.username) == 0) {
            if (users[i].has_applied) {
                printf("You have already applied for the exam.\n");
                return;
            }
            users[i].has_applied = true;
            users[i].application_status = PENDING;
            printf("Your exam application has been submitted for approval.\n");
            return;
        }
    }
    printf("Error: User not found!\n");
}

void view_exam_status() {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, current_user.username) == 0) {
            printf("\n=== Your Exam Application Status ===\n");
            printf("Username: %s\n", users[i].username);
            
            if (!users[i].has_applied) {
                printf("Status: Not applied yet\n");
                return;
            }
            
            switch (users[i].application_status) {
                case PENDING:
                    printf("Status: Pending approval\n");
                    break;
                case APPROVED:
                    printf("Status: Approved\n");
                    for (int j = 0; j < total_students; j++) {
                        if (strcmp(students[j].username, users[i].username) == 0) {
                            printf("Student ID: %d\n", students[j].id);
                            if (students[j].entered) {
                                printf("Exam Status: Taken in slot %d\n", students[j].exam_slot + 1);
                                printf("Room: %d\n", students[j].room_number + 1);
                            } else if (students[j].rejected) {
                                printf("Exam Status: Could not enter any exam slot\n");
                            } else {
                                printf("Exam Status: Not taken yet\n");
                            }
                            return;
                        }
                    }
                    printf("Exam Status: Approved but not yet scheduled\n");
                    break;
                case REJECTED:
                    printf("Status: Rejected\n");
                    break;
            }
            return;
        }
    }
    printf("Error: User not found!\n");
}

void display_pending_applications() {
    printf("\n=== Pending Applications ===\n");
    bool found = false;
    
    for (int i = 0; i < user_count; i++) {
        if (users[i].role == STUDENT && users[i].has_applied && users[i].application_status == PENDING) {
            printf("Username: %s\n", users[i].username);
            found = true;
        }
    }
    
    if (!found) {
        printf("No pending applications.\n");
    }
}

void approve_application(char *username) {
    bool user_exists = false;
    bool has_applied = false;
    int user_index = -1;
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            user_exists = true;
            user_index = i;
            if (users[i].has_applied) {
                has_applied = true;
            }
            break;
        }
    }
    
    if (!user_exists) {
        printf("Error: User '%s' not found in the system!\n", username);
        return;
    }
    
    if (!has_applied) {
        printf("Error: User '%s' has not applied for the exam!\n", username);
        return;
    }
    
    if (users[user_index].application_status != PENDING) {
        printf("Application for %s is not pending (current status: %s).\n", 
               username, 
               users[user_index].application_status == APPROVED ? "Approved" : "Rejected");
        return;
    }
    
    users[user_index].application_status = APPROVED;
    
    bool already_exists = false;
    for (int j = 0; j < total_students; j++) {
        if (strcmp(students[j].username, username) == 0) {
            already_exists = true;
            break;
        }
    }
    
    if (!already_exists) {
        Student* temp = (Student*)realloc(students, (total_students + 1) * sizeof(Student));
        if (!temp) {
            printf("Memory allocation failed!\n");
            return;
        }
        students = temp;
        
        Student* new_student = &students[total_students];
        new_student->id = AUTO_ID_BASE + total_students;
        new_student->room_number = -1;
        new_student->entered = false;
        new_student->left = false;
        new_student->rejected = false;
        new_student->exam_slot = -1;
        strcpy(new_student->username, username);
        
        total_students++;
        printf("Application approved and student %s added to exam list with ID %d\n", 
               username, new_student->id);
    } else {
        printf("Application approved (student already in exam list)\n");
    }
}

void reject_application(char *username) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            users[i].application_status = REJECTED;
            printf("Application for %s has been rejected.\n", username);
            return;
        }
    }
    printf("User %s not found!\n", username);
}

void auto_approve_all_applications() {
    int approved_count = 0;
    
    for (int i = 0; i < user_count; i++) {
        if (users[i].role == STUDENT && users[i].has_applied && users[i].application_status == PENDING) {
            users[i].application_status = APPROVED;
            
            bool exists = false;
            for (int j = 0; j < total_students; j++) {
                if (strcmp(students[j].username, users[i].username) == 0) {
                    exists = true;
                    break;
                }
            }
            
            if (!exists) {
                Student* temp = (Student*)realloc(students, (total_students + 1) * sizeof(Student));
                if (!temp) {
                    printf("Memory allocation failed for user %s!\n", users[i].username);
                    continue;
                }
                students = temp;
                
                Student* new_student = &students[total_students];
                new_student->id = AUTO_ID_BASE + total_students;
                new_student->room_number = -1;
                new_student->entered = false;
                new_student->left = false;
                new_student->rejected = false;
                new_student->exam_slot = -1;
                strcpy(new_student->username, users[i].username);
                
                total_students++;
                approved_count++;
            }
        }
    }
    
    printf("Auto-approved %d applications and added %d new students to exam list.\n", 
           approved_count, approved_count);
}

void view_all_applications() {
    printf("\n=== All Applications ===\n");
    printf("%-20s %-15s %-10s\n", "Username", "Applied", "Status");
    
    for (int i = 0; i < user_count; i++) {
        if (users[i].role == STUDENT) {
            printf("%-20s %-15s ", users[i].username, 
                   users[i].has_applied ? "Yes" : "No");
            
            if (!users[i].has_applied) {
                printf("N/A\n");
            } else {
                switch (users[i].application_status) {
                    case PENDING: printf("Pending\n"); break;
                    case APPROVED: printf("Approved\n"); break;
                    case REJECTED: printf("Rejected\n"); break;
                }
            }
        }
    }
}

void add_student_manually() {
    printf("\n=== Pending Applications ===\n");
    bool found_pending = false;
    
    for (int i = 0; i < user_count; i++) {
        if (users[i].role == STUDENT && users[i].has_applied && users[i].application_status == PENDING) {
            printf("Username: %s\n", users[i].username);
            found_pending = true;
        }
    }
    
    if (!found_pending) {
        printf("No pending applications to approve.\n");
        return;
    }
    
    char username[MAX_USERNAME_LEN];
    printf("\nEnter username to approve (or 'cancel' to abort): ");
    fgets(username, MAX_USERNAME_LEN, stdin);
    trim_newline(username);
    
    if (strcmp(username, "cancel") == 0) {
        printf("Operation cancelled.\n");
        return;
    }
    
    approve_application(username);
}

void student_menu() {
    int choice;
    
    while (1) {
        printf("\n=== Student Menu ===\n");
        printf("Welcome, %s!\n", current_user.username);
        printf("1. Apply for Exam\n");
        printf("2. View Exam Status\n");
        printf("3. Logout\n");
        printf("Enter your choice: ");
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input!\n");
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');
        
        switch (choice) {
            case 1:
                apply_for_exam();
                break;
            case 2:
                view_exam_status();
                break;
            case 3:
                return;
            default:
                printf("Invalid choice!\n");
        }
    }
}

void admin_menu() {
    int choice;
    
    while (1) {
        printf("\n=== Admin Menu ===\n");
        printf("1. Run Exam Simulation\n");
        printf("2. View Student Status\n");
        printf("3. Approve Student Manually\n");
        printf("4. Auto-Approve All Pending Applications\n");
        printf("5. View All Applications\n");
        printf("6. Logout\n");
        printf("Enter your choice: ");
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input!\n");
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');
        
        switch (choice) {
            case 1:
                run_simulation();
                break;
            case 2:
                view_student_status();
                break;
            case 3:
                add_student_manually();
                break;
            case 4:
                auto_approve_all_applications();
                break;
            case 5:
                view_all_applications();
                break;
            case 6:
                return;
            default:
                printf("Invalid choice!\n");
        }
    }
}

void view_student_status() {
    if (total_students == 0) {
        printf("No students registered for exam yet!\n");
        return;
    }
    
    printf("\n=== Student Status ===\n");
    printf("%-10s %-20s %-10s %-10s %-10s\n", 
           "ID", "Username", "Slot", "Room", "Status");
    
    for (int i = 0; i < total_students; i++) {
        printf("%-10d %-20s ", students[i].id, students[i].username);
        
        if (students[i].exam_slot == -1) {
            printf("%-10s ", "N/A");
        } else {
            printf("%-10d ", students[i].exam_slot + 1);
        }
        
        if (students[i].room_number == -1) {
            printf("%-10s ", "N/A");
        } else {
            printf("%-10d ", students[i].room_number + 1);
        }
        
        if (students[i].entered) {
            printf("Completed\n");
        } else if (students[i].rejected) {
            printf("Rejected\n");
        } else {
            printf("Pending\n");
        }
    }
}

bool try_lock(pthread_mutex_t *mutex) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += LOCK_TIMEOUT_SECONDS;
    
    int rc = pthread_mutex_timedlock(mutex, &ts);
    if (rc == 0) {
        return true;
    } else if (rc == ETIMEDOUT) {
        fprintf(stderr, "Warning: Mutex lock timeout\n");
        return false;
    } else {
        perror("pthread_mutex_timedlock");
        return false;
    }
}

void reset_rooms_for_second_slot() {
    for (int i = 0; i < num_rooms; i++) {
        rooms[1][i].current_capacity = 0;
    }
}

void* student_thread(void* arg) {
    Student* student = (Student*)arg;
    
    student->exam_slot = 0;
    sem_wait(&exam_start_sem[0]);
    
    if (!try_lock(&entry_lock[0])) {
        student->rejected = true;
        printf("Student %d (%s) couldn't enter first slot due to system overload\n", 
               student->id, student->username);
    } else {
        int assigned_room = -1;
        for (int i = 0; i < num_rooms; i++) {
            if (try_lock(&rooms[0][i].lock)) {
                if (rooms[0][i].current_capacity < rooms[0][i].max_capacity) {
                    rooms[0][i].current_capacity++;
                    assigned_room = i;
                    break;
                }
                pthread_mutex_unlock(&rooms[0][i].lock);
            }
        }
        
        if (assigned_room != -1) {
            student->room_number = assigned_room;
            student->entered = true;
            student->rejected = false;
            printf("Student %d (%s) entered Room %d in first slot\n", 
                   student->id, student->username, student->room_number + 1);
            pthread_mutex_unlock(&rooms[0][assigned_room].lock);
        } else {
            student->rejected = true;
            printf("Student %d (%s) couldn't find an available room in first slot\n", 
                   student->id, student->username);
        }
        
        pthread_mutex_unlock(&entry_lock[0]);
    }
    
    if (student->entered) {
        sleep(EXAM_DURATION_SECONDS);
        sem_wait(&exam_end_sem[0]);
        
        if (try_lock(&hall_lock)) {
            student->left = true;
            printf("Student %d (%s) left Room %d after first slot exam\n", 
                   student->id, student->username, student->room_number + 1);
            pthread_mutex_unlock(&hall_lock);
        }
        return NULL;
    }

    student->exam_slot = 1;
    sem_wait(&exam_start_sem[1]);
    
    if (!try_lock(&entry_lock[1])) {
        student->rejected = true;
        printf("Student %d (%s) couldn't enter second slot due to system overload\n", 
               student->id, student->username);
        return NULL;
    }
    
    int assigned_room = -1;
    for (int i = 0; i < num_rooms; i++) {
        if (try_lock(&rooms[1][i].lock)) {
            if (rooms[1][i].current_capacity < rooms[1][i].max_capacity) {
                rooms[1][i].current_capacity++;
                assigned_room = i;
                break;
            }
            pthread_mutex_unlock(&rooms[1][i].lock);
        }
    }
    
    if (assigned_room != -1) {
        student->room_number = assigned_room;
        student->entered = true;
        student->rejected = false;
        printf("Student %d (%s) entered Room %d in second slot\n", 
               student->id, student->username, student->room_number + 1);
        pthread_mutex_unlock(&rooms[1][assigned_room].lock);
    } else {
        student->rejected = true;
        printf("Student %d (%s) couldn't find an available room in second slot\n", 
               student->id, student->username);
    }
    
    pthread_mutex_unlock(&entry_lock[1]);
    
    if (student->entered) {
        sleep(EXAM_DURATION_SECONDS);
        sem_wait(&exam_end_sem[1]);
        
        if (try_lock(&hall_lock)) {
            student->left = true;
            printf("Student %d (%s) left Room %d after second slot exam\n", 
                   student->id, student->username, student->room_number + 1);
            pthread_mutex_unlock(&hall_lock);
        }
    }
    
    return NULL;
}

void* exam_supervisor_thread(void* arg) {
    int slot = *(int*)arg;
    time_t exam_start_time;
    time(&exam_start_time);
    printf("\n=== Exam Slot %d is starting now at %s", slot + 1, ctime(&exam_start_time));
    
    for (int i = 0; i < total_students; i++) {
        sem_post(&exam_start_sem[slot]);
    }
    
    sleep(EXAM_DURATION_SECONDS);
    
    time_t exam_end_time;
    time(&exam_end_time);
    printf("\n=== Exam Slot %d has ended at %s", slot + 1, ctime(&exam_end_time));
    
    for (int i = 0; i < total_students; i++) {
        sem_post(&exam_end_sem[slot]);
    }
    
    return NULL;
}

void print_summary() {
    printf("\n=== Exam Attendance Summary ===\n");
    printf("Total students registered: %d\n", total_students);
    
    int students_entered_slot1 = 0;
    int students_entered_slot2 = 0;
    int students_rejected_both = 0;
    
    for (int i = 0; i < total_students; i++) {
        if (students[i].entered && students[i].exam_slot == 0) {
            students_entered_slot1++;
        } else if (students[i].entered && students[i].exam_slot == 1) {
            students_entered_slot2++;
        } else {
            students_rejected_both++;
        }
    }
    
    printf("\nStudents who entered first slot exam: %d\n", students_entered_slot1);
    printf("Students who entered second slot exam: %d\n", students_entered_slot2);
    printf("Students who couldn't enter either slot: %d\n", students_rejected_both);
    
    if (students_rejected_both > 0) {
        printf("\nStudents who couldn't enter either slot:\n");
        for (int i = 0; i < total_students; i++) {
            if (!students[i].entered) {
                printf("Student %d (%s)\n", students[i].id, students[i].username);
            }
        }
    }
    
    printf("\nExam duration per slot: %d seconds (simulated)\n", EXAM_DURATION_SECONDS);
    printf("================================\n");
}

void print_welcome_message() {
    printf("\n=== IELTS/GRE Exam Hall Management System ===\n");
    printf("Welcome to the computerized exam management portal\n");
    printf("Please login or register to continue\n");
    printf("-----------------------------------------------\n");
}

void cleanup_resources() {
    for (int i = 0; i < NUM_EXAM_SLOTS; i++) {
        sem_destroy(&exam_start_sem[i]);
        sem_destroy(&exam_end_sem[i]);
        pthread_mutex_destroy(&entry_lock[i]);
        
        if (rooms[i]) {
            for (int j = 0; j < num_rooms; j++) {
                pthread_mutex_destroy(&rooms[i][j].lock);
            }
            free(rooms[i]);
        }
    }
    
    pthread_mutex_destroy(&hall_lock);
    
    if (students) {
        free(students);
    }
}

void run_simulation() {
    static bool rooms_initialized = false;
    
    if (!rooms_initialized) {
        while (1) {
            printf("Enter number of rooms: ");
            if (scanf("%d", &num_rooms) != 1 || num_rooms < 1) {
                printf("Invalid input. Please enter a positive number.\n");
                while (getchar() != '\n');
            } else break;
        }

        while (1) {
            printf("Enter room capacity (0 for default %d): ", DEFAULT_ROOM_CAPACITY);
            if (scanf("%d", &room_capacity) != 1 || room_capacity < 0) {
                printf("Invalid input. Please enter a non-negative number.\n");
                while (getchar() != '\n');
            } else {
                if (room_capacity == 0) room_capacity = DEFAULT_ROOM_CAPACITY;
                break;
            }
        }
        rooms_initialized = true;
    }

    for (int i = 0; i < NUM_EXAM_SLOTS; i++) {
        rooms[i] = (Room*)calloc(num_rooms, sizeof(Room));
        if (!rooms[i]) {
            printf("Error: Memory allocation failed!\n");
            return;
        }
    }
    
    for (int slot = 0; slot < NUM_EXAM_SLOTS; slot++) {
        for (int i = 0; i < num_rooms; i++) {
            rooms[slot][i].room_number = i;
            rooms[slot][i].current_capacity = 0;
            rooms[slot][i].max_capacity = room_capacity;
            pthread_mutex_init(&rooms[slot][i].lock, NULL);
        }
    }
    
    for (int i = 0; i < NUM_EXAM_SLOTS; i++) {
        sem_init(&exam_start_sem[i], 0, 0);
        sem_init(&exam_end_sem[i], 0, 0);
        pthread_mutex_init(&entry_lock[i], NULL);
    }
    pthread_mutex_init(&hall_lock, NULL);
    
    if (total_students == 0) {
        printf("No students registered for the exam!\n");
        printf("Please add students first.\n");
        return;
    }
    
    for (int i = 0; i < total_students; i++) {
        if (pthread_create(&students[i].thread, NULL, student_thread, &students[i]) != 0) {
            perror("Failed to create student thread");
            cleanup_resources();
            return;
        }
    }
    
    pthread_t supervisors[NUM_EXAM_SLOTS];
    int slot_numbers[NUM_EXAM_SLOTS] = {0, 1};
    
    if (pthread_create(&supervisors[0], NULL, exam_supervisor_thread, &slot_numbers[0]) != 0) {
        perror("Failed to create first slot supervisor thread");
        cleanup_resources();
        return;
    }
    
    pthread_join(supervisors[0], NULL);
    
    reset_rooms_for_second_slot();
    
    if (pthread_create(&supervisors[1], NULL, exam_supervisor_thread, &slot_numbers[1]) != 0) {
        perror("Failed to create second slot supervisor thread");
        cleanup_resources();
        return;
    }
    
    for (int i = 0; i < total_students; i++) {
        pthread_join(students[i].thread, NULL);
    }
    pthread_join(supervisors[1], NULL);
    
    print_summary();
    cleanup_resources();
}

int main() {
    initialize_default_admin();
    
    while (1) {
        print_welcome_message();
        
        int choice;
        printf("1. Login\n");
        printf("2. Register\n");
        printf("3. Exit\n");
        printf("Enter your choice: ");
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input!\n");
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');
        
        switch (choice) {
            case 1:
                if (login()) {
                    if (current_user.role == ADMIN) {
                        admin_menu();
                    } else {
                        student_menu();
                    }
                }
                break;
            case 2:
                register_user();
                break;
            case 3:
                cleanup_resources();
                printf("Exiting system. Goodbye!\n");
                return 0;
            default:
                printf("Invalid choice!\n");
        }
    }
    
    return 0;
}