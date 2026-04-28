#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/interrupt.h"
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
  
  struct list fds; 
  int next_fd;
};


/**
 * the file descriptor entry struct,  a fd <-> struct file*
 * - fd : file descriptor number
 * - file : the pointer to the struct file opened by this fd
 * - elme : the list element to put this struct into the list of fds in the PCB
 */
struct fd_entry{
   int fd;
   struct file* file;
   struct list_elem elem;
};

/**
 * the shared status used by fork
 * - fork_success : whether the forked thread load the ELF successfully
 * - fork_sema : sync the parent and child during fork
 */
struct fork_status{
   bool fork_success;
   struct semaphore fork_sema;
};
/**
 * The aux struct used to pass multiple argu to start_fork()
 * - pf : the copy of the parent intr_frame
 * - fs :  the shared status
 * - child_status: the forked process running/exit status 
 * - parent: thr info of the parent status, for later to copy
 */
struct fork_aux{
   struct intr_frame pf; // make a copy of intr_frame obj instead of a point
   struct fork_status* fs;
   struct child_status* child_status;
   struct thread* parent;
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

int process_alloc_fd(struct file *f);
struct file *process_get_file(int fd);
void process_close_fd(int fd);

pid_t process_fork(struct intr_frame* parent_if);
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
