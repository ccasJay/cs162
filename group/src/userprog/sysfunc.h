#include "threads/thread.h"
#include "userprog/process.h"

tid_t sys_pthread_create(stub_fun sfun, pthread_fun fun, void *arg);
tid_t sys_pthread_join(tid_t tid); // send the `SYS_PT_JOIN` syscall to the kernel
bool sys_lock_init(void* lock);
bool sys_lock_acquire(void* lock);
bool sys_lock_release(void* lock);

bool sys_sema_init(void* sema, int val);
bool sys_sema_up(void* sema);
bool sys_sema_down(void* sema);
