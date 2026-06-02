#include "list.h"
#include "stddef.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/sysfunc.h"



/**
 * @brief Find the user semaphore from the user_semas list
 * 
 * @param list 
 * @param user_addr 
 * @return struct user_sema*  
 */
struct user_sema* find_user_sema(struct list* list, void* user_addr){
  struct list_elem* e;
  for(e = list_begin(list); e != list_end(list); e = list_next(e)){
    struct user_sema* us = list_entry(e, struct user_sema, elem);
    if(us->user_addr == user_addr)return us;
  }
  return NULL;
}

/**
 * @brief Find the user lock form the userr_lock list
 * 
 * @param list 
 * @param user_addr 
 * @return struct user_lock* 
 */
 struct user_lock* find_user_lock(struct list* list, void* user_addr){
  struct list_elem* e;
  for(e = list_begin(list); e != list_end(list); e = list_next(e)){
    struct user_lock* ul = list_entry(e, struct user_lock, elem);
    if(ul->user_addr == user_addr)return ul;
  }
  return NULL;
}


tid_t sys_pthread_create(stub_fun sfun, pthread_fun tfun, void* arg) {
  if(!is_user_vaddr(sfun) || !is_user_vaddr(tfun))return TID_ERROR;
  return pthread_execute(sfun, tfun, arg);
}

tid_t sys_pthread_join(tid_t tid){
  if(tid == TID_ERROR)return TID_ERROR;
  return pthread_join(tid);
}

bool sys_sema_init(void *sema, int val){
  if(!is_user_vaddr(sema) || sema == NULL || val < 0)return false;
  struct user_sema* us = malloc(sizeof *us);
  if(us == NULL)return false;
  us->user_addr = sema; 
  sema_init(&us->sema,val);
  struct thread* t = thread_current();
  lock_acquire(&t->pcb->user_sync_lock);
  list_push_back(&t->pcb->user_semas,&us->elem);
  lock_release(&t->pcb->user_sync_lock);
  return true;
}

bool sys_sema_down(void *sema){
  if(!is_user_vaddr(sema))return false;
  struct thread* t = thread_current();
  lock_acquire(&t->pcb->user_sync_lock);
  struct user_sema* us = find_user_sema(&t->pcb->user_semas, sema);
  lock_release(&t->pcb->user_sync_lock);
  if(us == NULL)return false;
  sema_down(&us->sema);
  return true;
}

bool sys_sema_up(void *sema){
  if(!is_user_vaddr(sema))return false;
  struct thread* t = thread_current();
  lock_acquire(&t->pcb->user_sync_lock);
  struct user_sema* us = find_user_sema(&t->pcb->user_semas, sema);
  lock_release(&t->pcb->user_sync_lock);
  if(us == NULL)return false;
  sema_up(&us->sema);
  return true;
}

bool sys_lock_init(void *lock){
  if(!is_user_vaddr(lock) || lock == NULL)return false;
  struct user_lock* ul = malloc(sizeof *ul);
  if(ul == NULL)return false;
  ul->user_addr = lock;
  lock_init(&ul->lock);
  struct thread* t = thread_current();
  lock_acquire(&t->pcb->user_sync_lock);
  list_push_back(&t->pcb->user_locks, &ul->elem);
  lock_release(&t->pcb->user_sync_lock);
  return true;
}

bool sys_lock_acquire(void *lock){
  if(!is_user_vaddr(lock))return false;
  struct thread* t = thread_current();
  lock_acquire(&t->pcb->user_sync_lock);
  struct user_lock* ul = find_user_lock(&t->pcb->user_locks, lock);
  lock_release(&t->pcb->user_sync_lock);
  if(ul == NULL)return false;
  lock_acquire(&ul->lock);
  return true;
}

bool sys_lock_release(void *lock){
  if(!is_user_vaddr(lock))return false;
  struct thread* t = thread_current();
  lock_acquire(&t->pcb->user_sync_lock);
  struct user_lock* ul = find_user_lock(&t->pcb->user_locks, lock);
  if(ul == NULL)return false;
  lock_release(&t->pcb->user_sync_lock);
  lock_release(&ul->lock);
  return true;
}
