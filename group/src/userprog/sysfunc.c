#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "threads/vaddr.h"



tid_t sys_pthread_create(stub_fun sfun, pthread_fun tfun, const void* arg) {
  if(!is_user_vaddr(sfun) || !is_user_vaddr(tfun))return TID_ERROR;
  return pthread_execute(sfun, tfun, arg);
}

tid_t sys_pthread_join(tid_t tid){
  if(tid == TID_ERROR)return TID_ERROR;
  return pthread_join(tid);
}

bool sys_sema_init(void *sema, int val){
  if(!is_user_vaddr(sema))return false;
  struct user_sema* us = malloc(sizeof us);
  if(us == NULL)return false;
  us->user_addr = sema;
  sema_init(&us->sema,val);
  struct thread* t = thread_current();
  list_push_back(&t->pcb->semas,&us->elem);
  return true;
}