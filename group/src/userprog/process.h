#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <stdint.h>

// At most 8MB can be allocated to the stack
// These defines will be used in Project 2: Multithreading
#define MAX_STACK_PAGES (1 << 11)
#define MAX_THREADS 127

/* PIDs and TIDs are the same type. PID should be
   the TID of the main thread of the process */
typedef tid_t pid_t;

/* Thread functions (Project 2: Multithreading) */
typedef void (*pthread_fun)(void*);
typedef void (*stub_fun)(pthread_fun, void*);

/* The process control block for a given process. Since
   there can be multiple threads per process, we need a separate
   PCB from the TCB. All TCBs in a process will have a pointer
   to the PCB, and the PCB will have a pointer to the main thread
   of the process, which is `special`. */
struct process {
  /* Owned by process.c. */
  uint32_t* pagedir;          /* Page directory. */
  char process_name[16];      /* Name of the main thread */
  struct thread* main_thread; /* Pointer to main thread */

  struct list children;
  struct child_status *my_status;
};

/**
 * the shared status between father and children
 * - load_success: whether the children load the ELF successfully
 * - load_sema: used to sync the parent and child during exec
 */
struct exec_status{
   bool load_success;
   struct semaphore load_sema;
};

/**
 * The aux struct used to pass multiple arguments to start_process() when
 * creating a new process. 
 * - cmdline: the command line to execute
 * - load_status: package the shared status between parent and child during exec
 * - child_status: package the shared child process running/exit status between parent and child processes
 * 
 */
struct exec_aux{
   char* cmdline;
   struct exec_status *load_status;
   struct child_status *child_status;
};

/**
 * the struct to keep track of the children status in the parent process
 * 
 */
struct child_status{
   pid_t pid; 
   int exit_status;
   bool is_waited_on; // Whether the parent has called process_wait on this child yet
   struct semaphore wait_sema;// Semaphore to block parent until child exits
   struct list_elem elem;
};

void userprog_init(void);

pid_t process_execute(const char* file_name);
int process_wait(pid_t);
void process_exit(void);
void process_activate(void);

bool is_main_thread(struct thread*, struct process*);
pid_t get_pid(struct process*);

tid_t pthread_execute(stub_fun, pthread_fun, void*);
tid_t pthread_join(tid_t);
void pthread_exit(void);
void pthread_exit_main(void);

#endif /* userprog/process.h */
