/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "list.h"
#include "stdbool.h"
#include "stddef.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

static bool compare_semaphore_elem(const struct list_elem* a, const struct list_elem* b, void* aux UNUSED);
static bool donor_already_in(const struct list_elem* donor,const struct list* donor_list);

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void sema_init(struct semaphore* sema, unsigned value) {
  ASSERT(sema != NULL);

  sema->value = value;
  list_init(&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void sema_down(struct semaphore* sema) {
  enum intr_level old_level;

  ASSERT(sema != NULL);
  ASSERT(!intr_context());

  old_level = intr_disable();
  while (sema->value == 0) {
    list_insert_ordered(&sema->waiters, &thread_current()->elem, (list_less_func*) compare_prio, NULL);
    thread_block();
  }
  sema->value--;
  intr_set_level(old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool sema_try_down(struct semaphore* sema) {
  enum intr_level old_level;
  bool success;

  ASSERT(sema != NULL);

  old_level = intr_disable();
  if (sema->value > 0) {
    sema->value--;
    success = true;
  } else
    success = false;
  intr_set_level(old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void sema_up(struct semaphore* sema) {
  enum intr_level old_level;
  //定义一个bool用来检测是否需要yield 因为不能在临街区这中yield, 此处的=临界区伴随着关中断
  bool should_yield = false;

  ASSERT(sema != NULL);

  old_level = intr_disable();
  if (!list_empty(&sema->waiters)){
    //低优先级收到donation后在waiters list 中的位置变化了，需要重新排序，否则影响sema_up
    list_sort(&sema->waiters, (list_less_func*)compare_prio, NULL);
    struct thread* t = list_entry(list_pop_front(&sema->waiters), struct thread, elem);
    thread_unblock(t);
    if(t->priority > thread_current()->priority){
     should_yield  = true;
    }
  }
  sema->value++;
  intr_set_level(old_level);
  if(should_yield){
    if(intr_context()){
      intr_yield_on_return();
    }else thread_yield();
  }
}

static void sema_test_helper(void* sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test(void) {
  struct semaphore sema[2];
  int i;

  printf("Testing semaphores...");
  sema_init(&sema[0], 0);
  sema_init(&sema[1], 0);
  thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) {
    sema_up(&sema[0]);
    sema_down(&sema[1]);
  }
  printf("done.\n");
}

/* Thread function used by sema_self_test(). */
static void sema_test_helper(void* sema_) {
  struct semaphore* sema = sema_;
  int i;

  for (i = 0; i < 10; i++) {
    sema_down(&sema[0]);
    sema_up(&sema[1]);
  }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void lock_init(struct lock* lock) {
  ASSERT(lock != NULL);

  lock->holder = NULL;
  sema_init(&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void lock_acquire(struct lock* lock) {
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(!lock_held_by_current_thread(lock));
  struct thread* initial_t = thread_current();
  struct thread* cur_t = initial_t;
  struct thread* holder = lock->holder;
  enum intr_level old_level = intr_disable();
  if(holder != NULL){
    //目前导致阻塞的lock
    cur_t->waiting_on_lock = lock;
    if(!donor_already_in(&cur_t->donor_elem, &holder->donors)){
      list_push_back(&holder->donors, &cur_t->donor_elem);
    }
  }
  while(cur_t->waiting_on_lock != NULL){
    holder = cur_t->waiting_on_lock->holder;
    if(holder == NULL)break;
    if(cur_t->priority > holder->priority){
      holder->priority = cur_t->priority;
    }
    //update the cur_t to the thread that is holding the lock that cur_t is waiting on
    cur_t = holder;
  }
  intr_set_level(old_level);
  sema_down(&lock->semaphore);
  old_level = intr_disable();
  initial_t->waiting_on_lock = NULL;
  lock->holder = initial_t;

  /* Inherit the remaining waiters on this lock's semaphore.
     This thread is now the lock holder, so all threads still
     blocked on the semaphore should donate their priority to us. */
  {
    struct list* wait_list = &lock->semaphore.waiters;
    struct list_elem* e;
    for (e = list_begin(wait_list); e != list_end(wait_list);
         e = list_next(e)) {
      struct thread* w = list_entry(e, struct thread, elem);
      /* w is still waiting for this lock; ensure its donation
         now targets the new holder (initial_t). */
      w->waiting_on_lock = lock;
      if (!donor_already_in(&w->donor_elem, &initial_t->donors)) {
        list_push_back(&initial_t->donors, &w->donor_elem);
      }
      if (w->priority > initial_t->priority) {
        initial_t->priority = w->priority;
      }
    }
  }

  intr_set_level(old_level);
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool lock_try_acquire(struct lock* lock) { bool success;

  ASSERT(lock != NULL);
  ASSERT(!lock_held_by_current_thread(lock));

  success = sema_try_down(&lock->semaphore);
  if (success)
    lock->holder = thread_current();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.
    
   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void lock_release(struct lock* lock) {
  ASSERT(lock != NULL);
  ASSERT(lock_held_by_current_thread(lock));
  struct thread* cur_t = thread_current();
  enum intr_level old_level = intr_disable();
  // 删除lock产生的donors
  struct list* donors = &cur_t->donors;
  struct list_elem* e;
  int new_priority = cur_t->base_priority;
  for(e = list_begin(donors);e != list_end(donors);){
    struct list_elem* next = list_next(e);
    struct thread* t = list_entry(e, struct thread, donor_elem);
    if(t->waiting_on_lock == lock){
      list_remove(&t->donor_elem);
    }else{
      //重新计算新的 priority
      if(t->priority > new_priority){
        new_priority = t->priority;
      }
    }
    e = next;
  }
  cur_t->priority = new_priority;

  lock->holder = NULL;
  intr_set_level(old_level);
  sema_up(&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool lock_held_by_current_thread(const struct lock* lock) {
  ASSERT(lock != NULL);

  return lock->holder == thread_current();
}

/* Initializes a readers-writers lock */
void rw_lock_init(struct rw_lock* rw_lock) {
  lock_init(&rw_lock->lock);
  cond_init(&rw_lock->read);
  cond_init(&rw_lock->write);
  rw_lock->AR = rw_lock->WR = rw_lock->AW = rw_lock->WW = 0;
}

/* Acquire a writer-centric readers-writers lock */
void rw_lock_acquire(struct rw_lock* rw_lock, bool reader) {
  // Must hold the guard lock the entire time
  lock_acquire(&rw_lock->lock);

  if (reader) {
    // Reader code: Block while there are waiting or active writers
    while ((rw_lock->AW + rw_lock->WW) > 0) {
      rw_lock->WR++;
      cond_wait(&rw_lock->read, &rw_lock->lock);
      rw_lock->WR--;
    }
    rw_lock->AR++;
  } else {
    // Writer code: Block while there are any active readers/writers in the system
    while ((rw_lock->AR + rw_lock->AW) > 0) {
      rw_lock->WW++;
      cond_wait(&rw_lock->write, &rw_lock->lock);
      rw_lock->WW--;
    }
    rw_lock->AW++;
  }

  // Release guard lock
  lock_release(&rw_lock->lock);
}

/* Release a writer-centric readers-writers lock */
void rw_lock_release(struct rw_lock* rw_lock, bool reader) {
  // Must hold the guard lock the entire time
  lock_acquire(&rw_lock->lock);

  if (reader) {
    // Reader code: Wake any waiting writers if we are the last reader
    rw_lock->AR--;
    if (rw_lock->AR == 0 && rw_lock->WW > 0)
      cond_signal(&rw_lock->write, &rw_lock->lock);
  } else {
    // Writer code: First try to wake a waiting writer, otherwise all waiting readers
    rw_lock->AW--;
    if (rw_lock->WW > 0)
      cond_signal(&rw_lock->write, &rw_lock->lock);
    else if (rw_lock->WR > 0)
      cond_broadcast(&rw_lock->read, &rw_lock->lock);
  }

  // Release guard lock
  lock_release(&rw_lock->lock);
}

/* One semaphore in a list. */
struct semaphore_elem {
  struct list_elem elem;      /* List element. */
  struct semaphore semaphore; /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void cond_init(struct condition* cond) {
  ASSERT(cond != NULL);

  list_init(&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void cond_wait(struct condition* cond, struct lock* lock) {
  struct semaphore_elem waiter;

  ASSERT(cond != NULL);
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(lock_held_by_current_thread(lock));

  sema_init(&waiter.semaphore, 0);
  list_push_back(&cond->waiters, &waiter.elem);
  lock_release(lock);
  sema_down(&waiter.semaphore);
  lock_acquire(lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_signal(struct condition* cond, struct lock* lock UNUSED) {
  ASSERT(cond != NULL);
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(lock_held_by_current_thread(lock));

  if (!list_empty(&cond->waiters)) {
    list_sort(&cond->waiters, compare_semaphore_elem, NULL);
    struct semaphore_elem* waiter = list_entry(list_pop_front(&cond->waiters), struct semaphore_elem, elem);
    sema_up(&waiter->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_broadcast(struct condition* cond, struct lock* lock) {
  ASSERT(cond != NULL);
  ASSERT(lock != NULL);

  while (!list_empty(&cond->waiters))
    cond_signal(cond, lock);
}


/*(Helper) Compare the semaphore_elem used for list_insert_ordered()*/
static bool compare_semaphore_elem(const struct list_elem* a, const struct list_elem* b, void* aux UNUSED) {
  struct semaphore_elem* sa = list_entry(a, struct semaphore_elem, elem);
  struct semaphore_elem* sb = list_entry(b, struct semaphore_elem, elem);
  struct thread* ta = list_entry(list_front(&sa->semaphore.waiters), struct thread, elem);
  struct thread* tb = list_entry(list_front(&sb->semaphore.waiters), struct thread, elem);

  return ta->priority > tb->priority;

}

/*(Helper)Whether the donor is already in the donors list*/
static bool donor_already_in(const struct list_elem* donor,const struct list* donor_list){
  struct thread* t = list_entry(donor, struct thread, donor_elem);
  struct list_elem* e;
  for(e = list_begin(donor_list);e != list_end(donor_list);e = list_next(e)){
    struct thread* cur = list_entry(e, struct thread, donor_elem);
    if(cur == t)return true;
  }
  return false;
}