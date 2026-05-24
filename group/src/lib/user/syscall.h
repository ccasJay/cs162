#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include <stdbool.h>
#include <debug.h>
#include <pthread.h>
#include <stdlib.h>

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t)-1)

/* Synchronization Types */
typedef char lock_t;
typedef char sema_t;

/* Map region identifier. */
typedef int mapid_t;
#define MAP_FAILED ((mapid_t)-1)

/* Maximum characters in a filename written by readdir(). */
#define READDIR_MAX_LEN 14

/* Typical return values from main() and arguments to exit(). */
#define EXIT_SUCCESS 0 /* Successful execution. */
#define EXIT_FAILURE 1 /* Unsuccessful execution. */

/* Projects 2 and later. */
void halt(void) NO_RETURN;
void exit(int status) NO_RETURN;
pid_t exec(const char* file);
int wait(pid_t);
bool create(const char* file, unsigned initial_size);
bool remove(const char* file);
int open(const char* file);
int filesize(int fd);
int read(int fd, void* buffer, unsigned length);
int write(int fd, const void* buffer, unsigned length);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
int practice(int i);
double compute_e(int n);

/*Creates a new user thread running stub function sfun, with arguments tfun and arg. Returns TID of created thread, or TID_ERROR if allocation failed.*/
tid_t sys_pthread_create(stub_fun sfun, pthread_fun tfun, const void* arg);

/*Terminates the calling user thread. If the main thread calls pthread_exit, it should join on all currently active threads, and then exit the process.*/
void sys_pthread_exit(void) NO_RETURN;

/*Suspends the calling thread until the thread with TID tid finishes. Returns the TID of the thread waited on, or TID_ERROR if the thread could not be joined on. It is only valid to join on threads that are part of the same process and have not yet been joined on. It is valid to join on a thread that was part of the same process, but has already terminated – in such cases, the sys_pthread_join call should not block. Any thread can join on any other thread (the main thread included). If a thread joins on main, it should be woken up and allowed to run after main calls pthread_exit but before the process is killed*/
tid_t sys_pthread_join(tid_t tid);

/*Initializes lock, where lock is a pointer to a lock_t in userspace. Returns true if initialization was successful. You do not have to handle the case where lock_init is called on the same argument twice; you can assume that the result of doing so is undefined behavior.*/
bool lock_init(lock_t* lock);

/*Acquires lock, blocking if necessary, where lock is a pointer to a lock_t in userspace. Returns true if the lock was successfully acquired, false if the lock was not registered with the kernel in a lock_init call or if the current thread already holds the lock.*/
void lock_acquire(lock_t* lock);

/*Releases lock, where lock is a pointer to a lock_t in userspace. Returns true if the lock was successfully released, false if the lock was not registered with the kernel in a lock_init call or if the current thread does not hold the lock.*/
void lock_release(lock_t* lock);

/*Initializes sema to val, where sema is a pointer to a sema_t in userspace. Returns true if initialization was successful. You do not have to handle the case where sema_init is called on the same argument twice; you can assume that the result of doing so is undefined behavior.*/
bool sema_init(sema_t* sema, int val);

/*Downs sema, blocking if necessary, where sema is a pointer to a sema_t in userspace. Returns true if the semaphore was successfully downed, false if the semaphore was not registered with the kernel in a sema_init call.*/
void sema_down(sema_t* sema);

/*Ups sema, where sema is a pointer to a sema_t in userspace. Returns true if the sema was successfully upped, false if the sema was not registered with the kernel in a sema_init call.*/
void sema_up(sema_t* sema);

tid_t get_tid(void);

/* Project 3 and optionally project 4. */
mapid_t mmap(int fd, void* addr);
void munmap(mapid_t);

/* Project 4 only. */
bool chdir(const char* dir);
bool mkdir(const char* dir);
bool readdir(int fd, char name[READDIR_MAX_LEN + 1]);
bool isdir(int fd);
int inumber(int fd);

pid_t fork(void);

#endif /* lib/user/syscall.h */
