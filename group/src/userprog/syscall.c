#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/pagedir.h"
#include "threads/palloc.h"

static void syscall_handler(struct intr_frame*);

/*check whether the address is valid*/
static void check_address(const void *vaddr){
  if(!is_user_vaddr(vaddr) || !is_user_vaddr(vaddr+3)){
    process_exit();
  }
}


void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }

static void syscall_handler(struct intr_frame* f UNUSED) {
  uint32_t* args = ((uint32_t*)f->esp);

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */
  check_address(f->esp);
  //just a practice
  if(args[0] ==SYS_PRACTICE){
    check_address((void*)(args+1));
    f->eax = args[1] +1;
    return;
  }

  //halt
  if(args[0] == SYS_HALT){
    shutdown_power_off();
    return;
  }

  //exit 
  if (args[0] == SYS_EXIT) { 
    check_address((void*)(args+1));
    f->eax = args[1]; // the exit code;
    struct process *cur_pcb = thread_current()->pcb; // get current process's PCB
    printf("%s: exit(%d)\n", thread_current()->pcb->process_name, args[1]);

    if(cur_pcb->my_status!= NULL ){
      cur_pcb->my_status->exit_status = f->eax;
    }
    process_exit();
  }

  //exec
  if(args[0] == SYS_EXEC){
    check_address((void*)(args+1));
    char *u_cmdline = (char *) args[1];// the command line from user program
    check_address(u_cmdline);
    char *cmdline_cp = palloc_get_page(0);// a copy of the command line
    if(cmdline_cp == NULL){
      f->eax = TID_ERROR;
      return;
    }
    strlcpy(cmdline_cp,u_cmdline,PGSIZE);
    f->eax = process_execute(cmdline_cp);
    palloc_free_page(cmdline_cp);
    return;
  }

  //wait
  if(args[0] == SYS_WAIT){
    check_address((void*)(args+1));
    f->eax = process_wait(args[1]);
    return;
  }

  //fork
  if(args[0] == SYS_FORK){
    check_address((void*)(args+1));
    pid_t pid = args[1];
    
    f->eax = process_fork(f);
  }

}
