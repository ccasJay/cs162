#include "list.h"
#include "stdbool.h"
#include "stddef.h"
#include "threads/loader.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#define MAX_ARGS 256
static struct semaphore temporary; //TO CLEAN?
static thread_func start_process NO_RETURN; static thread_func start_fork;
static thread_func start_pthread NO_RETURN;
static bool load(const char* file_name, void (**eip)(void), void** esp);
static uint32_t* copy_pagedir(uint32_t* old_pd, uint32_t* new_pd);
bool setup_thread(void (**eip)(void), void** esp);
static bool copy_fds(struct process* parent, struct process* child);

/* Initializes user programs in the system by ensuring the main
   thread has a minimal PCB so that it can execute and wait for
   the first user process. Any additions to the PCB should be also
   initialized here if main needs those members */
void userprog_init(void) {
  struct thread* t = thread_current();
  bool success;

  /* Allocate process control block
     It is imoprtant that this is a call to calloc and not malloc,
     so that t->pcb->pagedir is guaranteed to be NULL (the kernel's
     page directory) when t->pcb is assigned, because a timer interrupt
     can come at any time and activate our pagedir */
  t->pcb = calloc(sizeof(struct process), 1);
  success = t->pcb != NULL;

  /* Kill the kernel if we did not succeed */
  ASSERT(success);

  list_init(&t->pcb->children);
  list_init(&t->pcb->fds);
  list_init(&t->pcb->pthreads);
  lock_init(&t->pcb->pthread_lock);
  t->pcb->next_fd = 2;
  t->pcb->executable = NULL;
}

/**
 * Fork: creates a copy of the calling process, including its file descriptors
 * and address space.
 * The calling process is the parent process, and the new process is the child
 * process.
 *
 * - In the parent process, returns the new process's PID.
 *   The new PID must be unique. If the program cannot load or run for any
 *   reason, returns -1.
 *
 * - In the child process, returns 0.
 *
 * - The child process is considered a direct child of the calling process
 *   (for wait()).
 *
 * - The child process gets a copy of the parent's file descriptors, which
 *   reference the same underlying struct file. STDIN and STDOUT do not need
 *   special handling here.
 */
pid_t process_fork(struct intr_frame* parent_if){
  /*Create the shared status*/
  struct fork_status* fork_status = malloc(sizeof(*fork_status));
  if(fork_status == NULL){
    return -1;
  }
  fork_status->fork_success = false;
  sema_init(&fork_status->fork_sema,0);

  /*Create the struct of child_status*/
  struct child_status* cs = malloc(sizeof(*cs));
  if(cs == NULL)return -1;
  cs->exit_status= -2;
  cs->is_waited_on = false;
  sema_init(&cs->wait_sema,0);

  /*The current thread who call the fork*/
  struct thread* t = thread_current();
  tid_t tid;

  /*Create the aux passed in the start_fork()*/
  struct fork_aux* aux = malloc(sizeof(*aux));
  if(aux == NULL){
    free(fork_status);
    return -1;
  }
  aux->child_status = cs;
  aux->fs = fork_status;
  aux->parent = t;
  aux->pf = *parent_if;
  list_push_back(&t->pcb->children,&cs->elem);

  /*Create a new thread*/
  tid = thread_create(t->name, PRI_DEFAULT, start_fork,aux);
  if(tid == TID_ERROR){
    free(fork_status);
    free(cs);
    free(aux);
    return -1;
  }
  cs->pid = tid;
  sema_down(&fork_status->fork_sema); // block until child finishes setting up its address space and fds
  if(!fork_status->fork_success){
    tid = -1;
  }
  free(fork_status);
  return tid;
}

static void start_fork(void* aux_passedin){
  /*Phrase the aux passed in*/
  struct fork_aux* aux = (struct fork_aux*)aux_passedin;
  struct child_status* cs = aux->child_status;

  struct fork_status* fork_status = aux->fs;
  struct thread* parent_t = aux->parent; 
  struct thread* current_t = thread_current();
  struct intr_frame if_ = aux->pf;
  bool success;
  
  /*Allocate process control block*/
  struct process* new_pcb = malloc(sizeof(struct process));
  success  = new_pcb != NULL;

  /*Initialize process control block*/
  if(success){
    new_pcb->pagedir = NULL;
    current_t->pcb = new_pcb;

    new_pcb->my_status=cs;
    new_pcb->main_thread = current_t;
    new_pcb->executable = NULL;
    list_init(&new_pcb->fds);
    list_init(&new_pcb->pthreads);
    list_init(&new_pcb->children);
    list_init(&new_pcb->user_semas);
    list_init(&new_pcb->user_locks);
    lock_init(&new_pcb->pthread_lock);
    lock_init(&new_pcb->user_sync_lock);
    /* Inherit the parent's thread name*/
    strlcpy(new_pcb->process_name, parent_t->name,sizeof(parent_t->name));
    
    new_pcb->pagedir = pagedir_create();
    success = new_pcb->pagedir != NULL;
    /* copy the parent process pagedir */
    if(success){
      success = copy_pagedir(parent_t->pcb->pagedir,new_pcb->pagedir) != NULL;
    }
    /* copy the entire file descriptor table from parent's pcb */
    if(success){
      success = copy_fds(parent_t->pcb,new_pcb);
    }
  }
  /* child pcb setup complete*/
  if (success) {
    thread_current()->pcb = new_pcb;
  }

  if(!success){
    fork_status->fork_success = false;
    /* Wake up the parent semaphore to avoid deadlock. */
    sema_up(&fork_status->fork_sema);
    free(aux);
    if(new_pcb != NULL && new_pcb->pagedir != NULL){
      pagedir_destroy(new_pcb->pagedir);
    }
    process_exit();
  }

  fork_status->fork_success = success;  
  /* Set return value for child process: fork returns 0 in the child. */
  if_.eax = 0;
  sema_up(&fork_status->fork_sema);
  free(aux);
  asm volatile("movl %0, %%esp; jmp intr_exit" : : "g"(&if_) : "memory");
  NOT_REACHED();
}
/**
 * (Helper)Copy one of a file's fd entry
 */
static struct fd_entry* fd_entry_copy(struct fd_entry* src){
  struct fd_entry* dst = malloc(sizeof(*dst));
  if(dst == NULL)
  return NULL;

  dst->fd = src->fd;
  lock_acquire(&filesys_lock);
  dst->file = file_duplicate(src->file);
  lock_release(&filesys_lock);
  if(dst->file == NULL){
    free(dst);
    return NULL;
  }
  return dst;
}

/**
 * (Helper)Copy a file's entire fd
 * - parent : parent process
 * - child : child process
 */
static bool copy_fds(struct process* parent, struct process* child){
  struct list_elem* e;

  list_init(&child->fds);
  child->next_fd = parent->next_fd;

  for(e=list_begin(&parent->fds);e != list_end(&parent->fds); e= list_next(e)){
    struct fd_entry* src = list_entry(e,struct fd_entry,elem);
    if(src->fd<2)continue;

    struct fd_entry* dst = fd_entry_copy(src);
    if(dst == NULL){
      return false;
    }
    list_push_back(&child->fds,&dst->elem);
  }
  return true;
}

/**
 * Assigns a new file descriptor for the given file in the current process
 * and returns the fd. Returns -1 if memory allocation fails.
 */
int process_alloc_fd(struct file* f) {
  struct thread* t = thread_current();
  struct fd_entry* entry = malloc(sizeof(*entry));
  if(entry == NULL){
    return -1;
  }

  entry->fd = t->pcb->next_fd++;
  entry->file = f;
  list_push_back(&t->pcb->fds,&entry->elem);
  return entry->fd;

}

/**
 * Looks up the file associated with a file descriptor in the current process.
 * Returns the struct file*, or NULL if not found.
 */
struct file* process_get_file(int fd) {
  struct thread* t = thread_current();
  struct list_elem* e;

  for(e = list_begin(&t->pcb->fds);e != list_end(&t->pcb->fds);e = list_next(e)){
    struct fd_entry* entry = list_entry(e,struct fd_entry,elem);
    if(entry->fd ==fd){
      return entry->file;
    }
  }
  return NULL;
}

/**
 * Closes the file associated with a file descriptor in the current process,
 * and removes its entry.
 */
void process_close_fd(int fd) {
  struct thread* t = thread_current();
  struct list_elem* e;

  for(e = list_begin(&t->pcb->fds);e != list_end(&t->pcb->fds);e = list_next(e)){
    struct fd_entry* entry =list_entry(e,struct fd_entry,elem);
    if(entry->fd == fd){
      lock_acquire(&filesys_lock);
      file_close(entry->file);
      lock_release(&filesys_lock);
      list_remove(&entry->elem);
      free(entry);
      return;
    }
  }
}

/**(Helper)Copy a pagedir
 * - use pagedir_create() to create the new pagedir
 * - use pagedir_get_page() and pagedir_set_page() to copy each page table entry,and map the vm to the pm
 * - if any step fails, free the new pagedir and return NULL
 */
static uint32_t* copy_pagedir(uint32_t* old_pd,uint32_t* new_pd){
  ASSERT(old_pd!=NULL);
  ASSERT(new_pd!=NULL);
  for (void* upage = 0; upage<(void*)PHYS_BASE;upage += PGSIZE){
    void *parent_kpage = pagedir_get_page(old_pd,upage);
    if(parent_kpage==NULL){
      continue;
    }
    void* child_kpage = palloc_get_page(PAL_USER);
    if(child_kpage == NULL){
      pagedir_destroy(new_pd);
      return NULL;
    } 
    bool writable = pagedir_is_writable(old_pd,upage);

    memcpy(child_kpage,parent_kpage,PGSIZE);
    if(!pagedir_set_page(new_pd,upage,child_kpage,writable)){
      palloc_free_page(child_kpage);
      pagedir_destroy(new_pd);
      return NULL;
    }
  }
  return new_pd;
}

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.Returns the new process's
   process id, or TID_ERROR if the thread cannot be created. */
pid_t process_execute(const char* file_name) {
  /*make a copy of file_name*/
  char* fn_copy;
  fn_copy = palloc_get_page(0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy(fn_copy, file_name, PGSIZE);

  /*create the shared status and init*/
  struct exec_status* status = malloc(sizeof(*status));
  if (status == NULL) {
    palloc_free_page(fn_copy);
    return TID_ERROR;
  }
  status->load_success = false;
  sema_init(&status->load_sema, 0);


  tid_t tid;
  sema_init(&temporary, 0);

  /*Create a copy of filename to use the first token as the thread name*/
  char name[128];
  char* save_str;
  strlcpy(name, file_name, sizeof(name));
  char* thread_name = strtok_r(name, " ", &save_str);

  /*Create the struct of child_status*/
  struct child_status* cs = malloc(sizeof(*cs));
  if(cs == NULL){
    palloc_free_page(fn_copy);
    return TID_ERROR;
  } 
  cs->exit_status=-2; //initialize to -2 to indicate the child is still running, since valid exit status is only 0-255
  cs->is_waited_on=false;
  sema_init(&cs->wait_sema,0);

  /*create the aux struct to package the multiplu arguments to start_process()*/
  struct exec_aux* aux = malloc(sizeof(*aux));
  if (aux == NULL) {
    palloc_free_page(fn_copy);
    free(status);
    return TID_ERROR;
  }
  aux->cmdline = fn_copy;
  aux->load_status = status;
  aux->child_status = cs;
  list_push_back(&thread_current()->pcb->children,&cs->elem);


  /* Create a new thread to execute FILE_NAME. */
  
  tid = thread_create(thread_name, PRI_DEFAULT, start_process, aux);
  if (tid == TID_ERROR) {
    palloc_free_page(fn_copy);
    free(aux);
    free(status);
    return TID_ERROR;
  }
  cs->pid = tid;

  sema_down(&status->load_sema);//father is block there until the child process load the ELF and give the loading result through the shared status struct
  if (!status->load_success)
    tid = TID_ERROR;
  free(status);
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */ 
static void start_process(void* file_name_) {
  //phrase the aux struct and extract the arguments
  struct exec_aux* aux = (struct exec_aux*)file_name_;
  struct child_status* cs = aux->child_status;
  
  char* file_name = aux->cmdline;
  struct exec_status* status = aux->load_status;

  struct thread* t = thread_current();
  struct intr_frame if_;
  bool success, pcb_success;
  /*Phrase the file_name into a temp arr*/
  char* token, *save_ptr;
  int argc =0;
  char *temp_argv[MAX_ARGS];
  for(token = strtok_r(file_name, " " , &save_ptr);token != NULL;token =strtok_r(NULL, " ",&save_ptr)){
    temp_argv[argc++] = token;
  }

  /* Allocate process control block */
  struct process* new_pcb = malloc(sizeof(struct process));
  success = pcb_success = new_pcb != NULL;

  /* Initialize process control block */
  if (success) {
    // Ensure that timer_interrupt() -> schedule() -> process_activate() // does not try to activate our uninitialized pagedir
    new_pcb->pagedir = NULL;
    t->pcb = new_pcb;
    t->pcb->my_status= cs;

    // Continue initializing the PCB as normal
    t->pcb->main_thread = t;
    strlcpy(t->pcb->process_name, t->name, sizeof t->name);
    list_init(&new_pcb->children);
    list_init(&new_pcb->fds);
    list_init(&new_pcb->pthreads);
    list_init(&new_pcb->user_locks);
    list_init(&new_pcb->user_semas);
    lock_init(&new_pcb->pthread_lock);
    lock_init(&new_pcb->user_sync_lock);
    new_pcb->next_fd = 2;
    new_pcb->executable = NULL;

    // Allocate a pthread_status to the main_thread
    struct pthread_status *main_status = malloc(sizeof(*main_status));
    main_status->tid = thread_tid();
    main_status->exited = false;
    main_status->joined = false;
    main_status->user_stack_page = NULL;
    sema_init(&main_status->join_sema, 0);

    thread_current()->pthread_status = main_status;
    list_push_back(&thread_current()->pcb->pthreads, &main_status->elem);

  }

  /* Initialize interrupt frame and load executable. */
  if (success) {
    memset(&if_, 0, sizeof if_);
    if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
    if_.cs = SEL_UCSEG;
    if_.eflags = FLAG_IF | FLAG_MBS;
    success = load(temp_argv[0],&if_.eip, &if_.esp); //change the `success` into the load check
      /* if load successed, Push arguments onto the stack in reverse order */
    if(success){
      char* arg_address[MAX_ARGS];
      /* 16-byte alignment (required for some tests and SSE) */
      if ((uintptr_t)if_.esp % 16 != 0) {
          if_.esp -= (uintptr_t)if_.esp % 16;
      }
      for(int i=argc-1;i>=0;i--){
        int len = strlen(temp_argv[i])+1;
        if_.esp -=len;
        //save the address of the argument on the stack for later use when we push argv
        memcpy(if_.esp,temp_argv[i],len);
        arg_address[i] = if_.esp;
      }

      /* 16-byte align the stack pointer before the arguments are pushed */
      uintptr_t target_before_ret = ((uintptr_t)if_.esp - (4 * argc + 12)) & ~0xF;
      int padding = (uintptr_t)if_.esp - (target_before_ret + 4 * argc + 12);
      if(padding!=0){
        if_.esp -=padding;
        memset(if_.esp,0,padding);
      }

      /* Push null pointer sentinel */
      if_.esp -=sizeof(char*);
      *((char **)if_.esp) = NULL;

      for(int i =argc -1;i>=0;i--){
        if_.esp -=sizeof(char *);
        *((char**)if_.esp) = arg_address[i];
      }
      
      /* Push argv (char**), argc (int), and fake return address (void*) */
      char **argv_start = (char**)if_.esp;
      if_.esp -=sizeof(char**);
      *((char***)if_.esp) = argv_start;

      if_.esp -= sizeof(int);
      *((int*)if_.esp) = argc;

      if_.esp -= sizeof(void*);
      *((void **)if_.esp) = NULL;
      }
    }

  status->load_success = success;
  sema_up(&status->load_sema);
  free(aux);

  /* Handle failure with succesful PCB malloc. Must free the PCB */
  if (!success && pcb_success) {
    // Avoid race where PCB is freed before t->pcb is set to NULL
    // If this happens, then an unfortuantely timed timer interrupt
    // can try to activate the pagedir, but it is now freed memory
    struct process* pcb_to_free = t->pcb;
    t->pcb = NULL;
    free(pcb_to_free);
  }

  /* Clean up. Exit on failure or jump to userspace */
  palloc_free_page(file_name);
  if (!success) {
    sema_up(&temporary);
    thread_exit();
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile("movl %0, %%esp; jmp intr_exit" : : "g"(&if_) : "memory");
  NOT_REACHED();
  }

/**
 * (Helper)Find the chasmild process in the children list by child_pid
 */
struct child_status* find_child_by_pid(struct list* children, pid_t target_pid){
  struct list_elem *e;
  for(e = list_begin(children); e!= list_end(children) ;e = list_next(e)){
    struct child_status *cs = list_entry(e,struct child_status,elem);
    if(cs->pid == target_pid){
      return cs;
    }
  }
  return NULL;
}

/* Waits for process with PID child_pid to die and returns its exit status.
   If it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If child_pid is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given PID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int process_wait(pid_t child_pid ) {
  /*get the current thread's pcb*/
  struct thread* cur = thread_current();
  if(cur->pcb == NULL){
    return -1;
  }

  /*check whether the child_pid is exist*/
  struct child_status *cs = find_child_by_pid(&cur->pcb->children, child_pid);
  if(cs == NULL)return -1;
  if(cs->is_waited_on) return -1;

  cs->is_waited_on = true;
  sema_down(&cs->wait_sema);
  list_remove(&cs->elem);
  int exit_status = cs->exit_status;
  free(cs);
  return exit_status;  
}

/* Free the current process's resources. */
void process_exit(void) {
  struct thread* cur = thread_current();
  uint32_t* pd;

  /* If this thread does not have a PCB, don't worry */
  if (cur->pcb == NULL) {
    thread_exit();
    NOT_REACHED();
  }

  /*the current process's child_status*/
  struct child_status* cs = cur->pcb->my_status;
  if (cs != NULL) {
    if (cs->exit_status == -2)
      cs->exit_status = -1;
    sema_up(&cs->wait_sema);
  }

  /* Close the executable file if it's still open */
  if (cur->pcb->executable != NULL) {
    lock_acquire(&filesys_lock);
    file_close(cur->pcb->executable);
    lock_release(&filesys_lock);
    cur->pcb->executable = NULL;
  }

  /* Close all open file descriptors */
  while (!list_empty(&cur->pcb->fds)) {
    struct list_elem *e = list_pop_front(&cur->pcb->fds);
    struct fd_entry *entry = list_entry(e, struct fd_entry, elem);
    lock_acquire(&filesys_lock);
    file_close(entry->file);
    lock_release(&filesys_lock);
    free(entry);
  }
  /* Free the user sync object*/
  while(!list_empty(&cur->pcb->user_locks)){
    struct list_elem *e = list_pop_front(&cur->pcb->user_locks);
    struct user_lock *ul = list_entry(e, struct user_lock, elem);
    free(ul);
  }
  while(!list_empty(&cur->pcb->user_semas)){
    struct list_elem *e = list_pop_front(&cur->pcb->user_semas);
    struct user_sema *us = list_entry(e, struct user_sema, elem);
    free(us);
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pcb->pagedir; if (pd != NULL) {
    /* Correct ordering here is crucial.  We must set
         cur->pcb->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
    cur->pcb->pagedir = NULL;
    pagedir_activate(NULL);
    pagedir_destroy(pd);
  }

  /* Free the PCB of this process and kill this thread
     Avoid race where PCB is freed before t->pcb is set to NULL
     If this happens, then an unfortuantely timed timer interrupt
     can try to activate the pagedir, but it is now freed memory */
  struct process* pcb_to_free = cur->pcb;
  cur->pcb = NULL;
  free(pcb_to_free);
  thread_exit();
}

/* Sets up the CPU for running user code in the current
   thread. This function is called on every context switch. */
void process_activate(void) {
  struct thread* t = thread_current();

  /* Activate thread's page tables. */
  if (t->pcb != NULL && t->pcb->pagedir != NULL)
    pagedir_activate(t->pcb->pagedir);
  else
    pagedir_activate(NULL);

  /* Set thread's kernel stack for use in processing interrupts.
     This does nothing if this is not a user process. */
  tss_update();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32 /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32 /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32 /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16 /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr {
  unsigned char e_ident[16];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr {
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL 0           /* Ignore. */
#define PT_LOAD 1           /* Loadable segment. */
#define PT_DYNAMIC 2        /* Dynamic linking info. */
#define PT_INTERP 3         /* Name of dynamic loader. */
#define PT_NOTE 4           /* Auxiliary info. */
#define PT_SHLIB 5          /* Reserved. */
#define PT_PHDR 6           /* Program header table. */
#define PT_STACK 0x6474e551 /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

static bool setup_stack(void** esp);
static bool validate_segment(const struct Elf32_Phdr*, struct file*);
static bool load_segment(struct file* file, off_t ofs, uint8_t* upage, uint32_t read_bytes,
                         uint32_t zero_bytes, bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool load(const char* file_name, void (**eip)(void), void** esp) {
  struct thread* t = thread_current();
  struct Elf32_Ehdr ehdr;
  struct file* file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pcb->pagedir = pagedir_create();
  if (t->pcb->pagedir == NULL)
    goto done;
  process_activate();

  /* Open executable file. */
  lock_acquire(&filesys_lock);
  file = filesys_open(file_name);
  if (file == NULL) {
    lock_release(&filesys_lock);
    printf("load: %s: open failed\n", file_name);
    goto done;
  }

  /* Read and verify executable header. */
  if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr ||
      memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7) || ehdr.e_type != 2 || ehdr.e_machine != 3 ||
      ehdr.e_version != 1 || ehdr.e_phentsize != sizeof(struct Elf32_Phdr) || ehdr.e_phnum > 1024) {
    printf("load: %s: error loading executable\n", file_name);
    goto done;
  }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) {
    struct Elf32_Phdr phdr;

    if (file_ofs < 0 || file_ofs > file_length(file))
      goto done;
    file_seek(file, file_ofs);

    if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
      goto done;
    file_ofs += sizeof phdr;
    switch (phdr.p_type) {
      case PT_NULL:
      case PT_NOTE:
      case PT_PHDR:
      case PT_STACK:
      default:
        /* Ignore this segment. */
        break;
      case PT_DYNAMIC:
      case PT_INTERP:
      case PT_SHLIB:
        goto done;
      case PT_LOAD:
        if (validate_segment(&phdr, file)) {
          bool writable = (phdr.p_flags & PF_W) != 0;
          uint32_t file_page = phdr.p_offset & ~PGMASK;
          uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
          uint32_t page_offset = phdr.p_vaddr & PGMASK;
          uint32_t read_bytes, zero_bytes;
          if (phdr.p_filesz > 0) {
            /* Normal segment.
                     Read initial part from disk and zero the rest. */
            read_bytes = page_offset + phdr.p_filesz;
            zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes);
          } else {
            /* Entirely zero.
                     Don't read anything from disk. */
            read_bytes = 0;
            zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
          }
          if (!load_segment(file, file_page, (void*)mem_page, read_bytes, zero_bytes, writable))
            goto done;
        } else
          goto done;
        break;
    }
  }

  /* Set up stack. */
  if (!setup_stack(esp))
    goto done;

  /* Start address. */
  *eip = (void (*)(void))ehdr.e_entry;

  success = true;

done:
  /* We arrive here whether the load is successful or not. */
  if (success) {
    /* Keep the executable open and deny writes to prevent modification. */
    t->pcb->executable = file;
    file_deny_write(file);
  } else {
    if (file != NULL) {
      file_close(file);
    }
  }
  
  if (file != NULL) {
    lock_release(&filesys_lock);
  }
  return success;
}

/* load() helpers. */

static bool install_page(void* upage, void* kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool validate_segment(const struct Elf32_Phdr* phdr, struct file* file) {
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off)file_length(file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr((void*)phdr->p_vaddr))
    return false;
  if (!is_user_vaddr((void*)(phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool load_segment(struct file* file, off_t ofs, uint8_t* upage, uint32_t read_bytes,
                         uint32_t zero_bytes, bool writable) {
  ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT(pg_ofs(upage) == 0);
  ASSERT(ofs % PGSIZE == 0);

  file_seek(file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) {
    /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    /* Get a page of memory. */
    uint8_t* kpage = palloc_get_page(PAL_USER);
    if (kpage == NULL)
      return false;

    /* Load this page. */
    if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes) {
      palloc_free_page(kpage);
      return false;
    }
    memset(kpage + page_read_bytes, 0, page_zero_bytes);

    /* Add the page to the process's address space. */
    if (!install_page(upage, kpage, writable)) {
      palloc_free_page(kpage);
      return false;
    }

    /* Advance. */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    upage += PGSIZE;
  }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool setup_stack(void** esp) {
  uint8_t* kpage;
  bool success = false;

  kpage = palloc_get_page(PAL_USER | PAL_ZERO);
  if (kpage != NULL) {
    success = install_page(((uint8_t*)PHYS_BASE) - PGSIZE, kpage, true);
    if (success)
      *esp = PHYS_BASE;
    else
      palloc_free_page(kpage);
  }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool install_page(void* upage, void* kpage, bool writable) {
  struct thread* t = thread_current();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page(t->pcb->pagedir, upage) == NULL &&
          pagedir_set_page(t->pcb->pagedir, upage, kpage, writable));
}

/* Returns true if t is the main thread of the process p */
bool is_main_thread(struct thread* t, struct process* p) { return p->main_thread == t; }

/* Gets the PID of a process */
pid_t get_pid(struct process* p) { return (pid_t)p->main_thread->tid; }

/**
 * @brief (Helper)find the free stack page address
 */
 void* find_free_stack_page(struct process* pcb){
    for(int i = 2; i < MAX_STACK_PAGES; i++){
      void* upage = (uint8_t*)PHYS_BASE - i * PGSIZE;

      if(pagedir_get_page(pcb->pagedir, upage) == NULL)return upage;
    }
    return NULL;
 }

/* Creates a new stack for the thread and sets up its arguments.
   Stores the thread's entry point into *EIP and its initial stack
   pointer into *ESP. Handles all cleanup if unsuccessful. Returns
   true if successful, false otherwise.

   This function will be implemented in Project 2: Multithreading. For
   now, it does nothing. You may find it necessary to change the
   function signature. */
bool setup_thread(void (**eip)(void) , void** esp ) { 
  uint8_t* kpage;
  bool success = false;

  kpage = palloc_get_page(PAL_USER | PAL_ZERO);
  if(kpage != NULL){
    void* upage = find_free_stack_page(thread_current()->pcb);
    if(upage == NULL)return false;
    success = install_page(upage, kpage, true);
    if(!success){
      palloc_free_page(kpage);
      return false;
    }
    *esp = (uint8_t*) upage + PGSIZE;
    thread_current()->pthread_status->user_stack_page = upage;
    return true;
  }
  return false;
}


/* Starts a new thread with a new user stack running SF, which takes
   TF and ARG as arguments on its user stack. This new thread may be
   scheduled (and may even exit) before pthread_execute () returns.
   Returns the new thread's TID or TID_ERROR if the thread cannot
   be created properly.

   This function will be implemented in Project 2: Multithreading and
   should be similar to process_execute (). For now, it does nothing.
   */
tid_t pthread_execute(stub_fun sf , pthread_fun tf , void* arg ) {
  struct user_thread_args* args = malloc(sizeof(*args));
  if(args == NULL)return TID_ERROR;
  args->sfun = sf;
  args->fun = tf;
  args->arg = arg;
  args->pcb = thread_current()->pcb;

  struct pthread_status *status = malloc(sizeof(*status));
  if(status == NULL){
    free(status);
    return TID_ERROR;
  }

  status->tid = TID_ERROR;
  status->exited = false;
  status->joined = false;
  sema_init(&status->join_sema, 0);
  args->status = status;

  tid_t tid = thread_create("pthread", PRI_DEFAULT, start_pthread, args);
  if(tid == TID_ERROR){
    free(args);
    free(status);
    return TID_ERROR;
  }
  status->tid = tid;
  list_push_back(&thread_current()->pcb->pthreads,&status->elem);
  return tid;
}


/* A thread function that creates a new user thread and starts it
   running. Responsible for adding itself to the list of threads in
   the PCB.

   This function will be implemented in Project 2: Multithreading and
   should be similar to start_process (). For now, it does nothing. */
static void start_pthread(void* args_) {
  //prase the argument passed
  struct user_thread_args *args =args_;
  stub_fun sfun = args->sfun;
  pthread_fun tfun = args->fun;
  void* arg = args->arg;
  struct process* pcb = args->pcb;
  thread_current()->pthread_status = args->status;

  //share the pcb with the parent pcb
  thread_current()->pcb = pcb;
  process_activate();

  //Initialize interrupt frame 
  struct intr_frame if_;
  memset(&if_, 0, sizeof(if_));
  //整个intr_fram清零
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  bool success = setup_thread(&if_.eip, &if_.esp);
  if(!success){
    free(args);
    thread_exit();
  }

  // Cast the ELF entry (sfun) address to the function-pointer type expected by intr_frame.
  if_.eip = (void(*)(void))sfun;

  // 16-byte align the stack pointer before the arguments are pushed
  uintptr_t target_before_ret = ((uintptr_t)if_.esp - 12) & ~0xF;
  int padding = (uintptr_t)if_.esp - (target_before_ret + 12);
  if(padding != 0){
    if_.esp -= padding;
  }

  //push the arguments to stack
  if_.esp -= sizeof(void*);
  *(void **) if_.esp = arg;

  if_.esp -= sizeof(pthread_fun);
  *(pthread_fun*) if_.esp = tfun;

  //fake return address
  if_.esp -= sizeof(void*);
  *(void **) if_.esp = NULL;

  free(args);

  //asm volatile("汇编" : 输出 : 输入 : clobber);
  //switch to user-mode from kernel-mode (the `iret` see SEL_UCSEG, it will switch the kernel privilege level to the user privilege level)
  asm volatile("movl %0, %%esp; jmp intr_exit" : : "g"(&if_) : "memory");
  NOT_REACHED();

}
/**
 * @brief (Helper)find the pthread status by tid
 * 
 */
static struct pthread_status* find_pthread_status(struct process* pcb, tid_t tid){
  struct list_elem *e;
  for(e = list_begin(&pcb->pthreads);e != list_end(&pcb->pthreads);){
    struct list_elem *next = list_next(e);
    struct pthread_status *ps = list_entry(e, struct pthread_status, elem);
    if(ps->tid == tid)return ps;
    e = next;
  }
  return NULL;
}

 
/* Waits for thread with TID to die, if that thread was spawned
   in the same process and has not been waited on yet. Returns TID on
   success and returns TID_ERROR on failure immediately, without
   waiting.

   This function will be implemented in Project 2: Multithreading. For
   now, it does nothing. */
tid_t pthread_join(tid_t tid ) { 
    if(tid == thread_tid())return TID_ERROR;

    struct process* pcb = thread_current()->pcb;
    lock_acquire(&pcb->pthread_lock);
    struct pthread_status *ps = find_pthread_status(pcb, tid);
    if(ps == NULL || ps->joined){
      lock_release(&pcb->pthread_lock);
      return TID_ERROR;
    }
    ps->joined = true;
    lock_release(&pcb->pthread_lock);
    sema_down(&ps->join_sema);
    return tid;
 }

/* Free the current thread's resources. Most resources will
   be freed on thread_exit(), so all we have to do is deallocate the
   thread's userspace stack. Wake any waiters on this thread.

   The main thread should not use this function. See
   pthread_exit_main() below.

   This function will be implemented in Project 2: Multithreading. For
   now, it does nothing. */
void pthread_exit(void) {
  struct thread *cur_t = thread_current();
  struct process *pcb = cur_t->pcb;
  struct pthread_status* status = cur_t->pthread_status;
  void* upage = status->user_stack_page;
  //free the user stack
  if(upage != NULL){
    void* kpage = pagedir_get_page(pcb->pagedir, upage);
    if(kpage != NULL){
      pagedir_clear_page(pcb->pagedir, upage);
      palloc_free_page(kpage);
    }
    status->user_stack_page = NULL;
  }

  lock_acquire(&pcb->pthread_lock);
  status->exited = true;
  lock_release(&pcb->pthread_lock);
  sema_up(&status->join_sema);
  thread_exit();
}
/**
 * @brief (Helper)Find a thread which is not the main_thread and unjoined
 * @param pcb 
 */
 struct pthread_status* find_unjoined_t(struct process* pcb){
    struct list_elem* e;
    for(e = list_begin(&pcb->pthreads); e != list_end(&pcb->pthreads);){
      struct list_elem* next = list_next(e);
      struct pthread_status* ps = list_entry(e, struct pthread_status, elem);

      bool nonmain_t = pcb->main_thread->pthread_status != ps;
      bool is_unjoined = !ps->joined ;

      if( nonmain_t && is_unjoined )return ps;
      e = next;
    }
    return NULL;
 }

/* Only to be used when the main thread explicitly calls pthread_exit.
   The main thread should wait on all threads in the process to
   terminate properly, before exiting itself. When it exits itself, it
   must terminate the process in addition to all necessary duties in
   pthread_exit.

   This function will be implemented in Project 2: Multithreading. For
   now, it does nothing. */
void pthread_exit_main(void) {
  // check whether the cur_t is the main_thread
  struct thread* cur_t = thread_current();
  struct process* pcb = cur_t->pcb;
  if(!is_main_thread(cur_t, pcb)){
    pthread_exit();
    NOT_REACHED();
  }

  //mark the main_status is exited
  struct pthread_status* main_status = cur_t->pthread_status;
  lock_acquire(&pcb->pthread_lock);
  main_status->exited = true;
  lock_release(&pcb->pthread_lock);
  sema_up(&main_status->join_sema);

  //wait for other threads to finish
  while(true){
    lock_acquire(&pcb->pthread_lock);
    struct pthread_status* target = find_unjoined_t(pcb);
    if(target == NULL){
      lock_release(&pcb->pthread_lock);
      break;
    }
    target->joined = true;
    lock_release(&pcb->pthread_lock);
    sema_down(&target->join_sema);

    //防止main thread被意外free
    if(target != pcb->main_thread->pthread_status){
      lock_acquire(&pcb->pthread_lock);
      list_remove(&target->elem);
      lock_release(&pcb->pthread_lock);
      if(pcb->my_status!= NULL){
        pcb->my_status->exit_status = 0;
        printf("%s: exit(%d)\n", pcb->main_thread->name, pcb->my_status->exit_status);
      }
      free(target);
    }
  }
  process_exit();
}
