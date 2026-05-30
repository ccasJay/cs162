# Project 2 Design Document

---

## Task 1: Barebones User-Level Thread Creation

**Goal:** Implement a basic version of `pthread_create` and `pthread_execute` that passes `tests/userprog/multithreading/create-simple`.

### 1. Data Structures and Functions

#### Modified Functions

```c
tid_t pthread_create(stub_fun sfun, pthread_fun fun, void *arg);
tid_t sys_pthread_create(stub_fun sfun, pthread_fun fun, void *arg);
static void pthread_execute(void *aux);

//thread join
bool pthread_join(tid_t tid); //user-level wrapper
tid_t sys_pthread_join(tid_t tid); // send the `SYS_PT_JOIN` syscall to the kernel
tid_t pthread_join(tid_t tid);//kernel side 
```

- `pthread_create()` is the user-level wrapper for the pthread create syscall.
- `sys_pthread_create()` validates the user arguments, builds the startup argument structure, and calls `thread_create()`.
- `pthread_execute()` is the kernel thread entry function passed to `thread_create()`, because `thread_create()` cannot run a user `pthread_fun` directly.
- On success, `pthread_create()` and `sys_pthread_create()` return the new thread's tid. On failure, they return `TID_ERROR`.

#### New Struct

```c
struct user_thread_args {
    stub_fun sfun;    /* User stub entry function. */
    pthread_fun fun;  /* Actual user thread function. */
    void *arg;        /* Argument passed to fun. */
    struct process* pcb;
};

// the shared status between user-thread
struct pthread_status{
    tid_t tid;
    bool exited;
    bool joined;
    struct semaphore join_sema;
    struct list_elem elem;
}
```

- The `user_thread_args` struct packages the three user arguments into the single `aux` pointer accepted by `thread_create()`.
- The kernel stores these as user virtual addresses and passes them to `pthread_execute()`; it does not call the user function directly.
- The `pthread_status` is added to track the lifetime of each user thread inside on process.
### 2. Algorithms
- The user program cannot call kernel functions directly, so `pthread_create()`
  works as a user-level wrapper. It invokes the `SYS_PTHREAD_CREATE` system call
  with `sfun`, `fun`, and `arg`. The common `syscall_handler` dispatches this
  syscall to `sys_pthread_create()`, which performs the actual thread creation
  in the kernel.
- Inside `sys_pthread_create()` , the kernel check the args is valid at first, then it allocates a `struct user_thread_args` and passes this struct as the `aux` argument to `thread_create` in kernel
- When the main thread calls `pthread_join(tid)` ,the user-level wrapper invokes the `SYS_PT_JOIN` syscall. Then the syscall handler dispatches it to the kernel-side `pthread_join`.In task 1 ,it just need to block until the created thread finish. (semaphore)


### 3. Synchronization

Threads in the same process share the PCB and address space, so `pthread_join` and `pthread_exit` must synchronize through shared per-thread status records. Each thread has a status object containing its TID, whether it has exited, whether it has already been joined, and a semaphore initialized to 0.

When `pthread_join(tid)` is called, the caller checks that the target thread is in the same process and has not already been joined, marks it as joined, and waits on the target thread's semaphore. When the target thread calls `pthread_exit`, it marks itself as exited and wakes any joining thread with `sema_up`.

A lock protects the process thread list and the joined/exited fields so that joining and exiting cannot race. This guarantees that a thread is joined at most once and that `pthread_join` only returns after the target thread has finished.

### 4. Rationale
- `pthread_create()` is the user-level entry point, so it cannot call kernel
  functions directly. Using `sys_pthread_create()` as the kernel-side
  implementation keeps the user/kernel boundary clear.
- The kernel already provides `thread_create()` for creating kernel threads, so
  reusing it avoids building a separate thread creation mechanism for user
  threads.
- `pthread_execute()` is needed as a bridge because `thread_create()` expects a
  kernel thread function, while the target `fun` is a user-level function
  pointer.


-----------------
## Task 2: User-level Synchronization Syscalls(lock and seamphore)
### 1. Data Structures and Functions
#### Modified function
```c
// userprog syscall.c
// seam syscall handler
if(args[0] == SYS_SEMA_INIT){};
if(args[0] == SYS_SEMA_DOWN){};
if(args[0] == SYS_SEMA_UP){};

//lock syscall handler
if(args[0] == SYS_LOCK_INIT){};
if(args[0] == SYS_LOCK_ACQUIRED){};
if(args[0] == SYS_LOCK_RELEASE){};

if(args[0] == SYS_GET_TID){};
```


#### New Struct
```c
struct user_sema{
  void *user_addr;
  struct semaphore sema;
  struct list_elem elem;
}

struct user_lock{
  void *user_addr;
  struct lock lock;
  struct list_elem elem;
}
```
#### Modified Struct
```c
struct process{
  ...
  struct list user_locks;
  struct list user_semas;
  struct lock user_sync_lock; //protect the user-level sync control table
}
```

#### New funciton
```c
bool sys_lock_init(void* lock);
bool sys_lock_acquire(void* lock);
bool sys_lock_release(void* lock);

bool sys_sema_init(void* sema, int val);
bool sys_sema_up(void* sema);
bool sys_sema_down(void* sema);

tid_t sys_get_tid();
```
- The particular logic of `sys_*` functions moved to the `userprog/sysfunc.c` 

### 2. Algorithms
- In the syscall_handler , first check the address when the syscall is called ,then call the corresponding `sys_*` function which will call the kernel function such as `sema_up()`
- `sys_get_tid()` called by `get_tid()` in the user-level, then it call the kernel-level function ` thread_tid()` 

### 3. Synchronization
- Add the `struct lock user_sync_lock` to protect the user-level synchronization control tables (`user_locks` and `user_semas`) in the process struct. This ensures that multiple threads can safely create and manipulate user-level locks and semaphores
