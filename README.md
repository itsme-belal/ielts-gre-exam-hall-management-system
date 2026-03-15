# IELTS / GRE Exam Hall Management System

A concurrent exam hall simulation system built using **C Programming**, **POSIX Threads**, **Semaphores**, and **Mutex Locks**.  
This project simulates how IELTS and GRE exam centers manage students, approve exam applications, allocate rooms, and run exams in multiple slots.

This project was developed as part of the **CSE325 - Operating Systems** course at **East West University**.

---

# Project Information

Course: CSE325 – Operating Systems  
Project: IELTS / GRE Exam Hall Management System  

Group Members:
- Belal Hossain — 2023-2-60-010
- Esrat Jahan Samiya — 2023-2-60-002
- Nusrat Jahan — 2023-2-60-286

---

# Project Overview

This system simulates a **realistic exam hall management system** where students apply for exams and administrators approve or reject their applications.

Once approved, students participate in a simulated exam where:

- Students attempt to enter exam halls
- Room capacity is enforced
- Multiple exam slots are managed
- Concurrent access is handled safely

The system uses **multithreading and synchronization techniques** to simulate many students attempting to take exams at the same time.

---

# Key Features

## User Authentication
- Student registration system
- Secure login for students and admins
- Default admin account

Admin credentials:

Username: admin
Password: admin123


---

## Exam Application System

Students can:
- Apply for an exam
- Check application status

Admins can:
- Approve applications
- Reject applications
- Auto-approve all pending applications

Application states:
- Pending
- Approved
- Rejected

---

## Exam Simulation

The exam system simulates real exam participation using **threads**.

Each student becomes a **separate thread** that tries to enter the exam hall.

Students attempt:

1️⃣ Slot 1 exam  
2️⃣ Slot 2 exam (if Slot 1 failed)

If both fail → student is rejected.

---

## Room Allocation System

Rooms have limited capacity.

When a student tries to enter:

1. The system searches for an available room
2. If capacity is available → student enters
3. If all rooms are full → student retries in Slot 2

If both slots are full → student cannot take the exam.

---

## Threading and Synchronization

The system uses several Operating System concepts:

### Threads
Each student is represented by a **separate thread**.

### Semaphores
Used to synchronize exam start and exam end.


exam_start_sem
exam_end_sem


### Mutex Locks
Used to avoid race conditions during:

- Room allocation
- Entry management
- Hall access

pthread_mutex_t


### Timeout Lock
The system uses a timed mutex lock to avoid deadlocks.

---

# System Workflow

### Student Flow

Register
↓
Login
↓
Apply for Exam
↓
Wait for Admin Approval
↓
Participate in Exam Simulation

---

### Admin Flow


Login
↓
View Applications
↓
Approve / Reject Students
↓
Run Exam Simulation
↓
View Exam Statistics


---

# Exam Simulation Logic

The simulation works as follows:

1. Admin starts the simulation
2. Student threads are created
3. Exam Slot 1 begins
4. Students attempt room allocation
5. Exam runs for a fixed time
6. Slot 1 ends
7. Rooms reset
8. Slot 2 begins
9. Remaining students retry
10. Final statistics are generated

---

# Output Example


Student 10001 (Alice) entered Room 3 in first slot
Student 10002 (Bob) couldn't find an available room in first slot
Student 10002 (Bob) entered Room 1 in second slot


Final summary:


Students who entered first slot: X
Students who entered second slot: X
Students rejected: X


---

# Technologies Used

- C Programming
- POSIX Threads (pthread)
- Semaphores
- Mutex Locks
- Multithreading
- Concurrency Control

---

# Compilation

To compile the program:


gcc ielts_gre.c -o exam -lpthread


Run the program:


./exam


---

# Learning Outcomes

This project demonstrates key **Operating System concepts**:

- Multithreading
- Synchronization
- Deadlock prevention
- Race condition handling
- Resource allocation
- Concurrent system simulation

---

# Future Improvements

Possible improvements for the system:

- Add GUI interface
- Integrate database for persistent storage
- Web-based exam management portal
- Real-time dashboard for admins
- Dynamic exam scheduling

---

# License

This project is created for academic purposes as part of the **Operating Systems course at East West University**.

---

# Author

Belal Hossain  
CSE Student  
East West University  
Bangladesh
