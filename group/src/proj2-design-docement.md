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
};
```

- This struct packages the three user arguments into the single `aux` pointer accepted by `thread_create()`.
- The kernel stores these as user virtual addresses and passes them to `pthread_execute()`; it does not call the user function directly.

### 2. Algorithms
- The user program cannot call kernel functions directly, so `pthread_create()`
  works as a user-level wrapper. It invokes the `SYS_PTHREAD_CREATE` system call
  with `sfun`, `fun`, and `arg`. The common `syscall_handler` dispatches this
  syscall to `sys_pthread_create()`, which performs the actual thread creation
  in the kernel.
- Inside `sys_pthread_create()` , the kernel check the args is valid at first, then it allocates a `struct user_thread_args` and passes this struct as the `aux` argument to `thread_create` in kernel


### 3. Synchronization
- Don't require the sync

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
