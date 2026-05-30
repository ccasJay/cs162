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
#include "string.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "userprog/sysfunc.h"

static void syscall_handler(struct intr_frame*);

/*check whether the address is valid*/
static void check_address(const void *vaddr){
  if(vaddr == NULL || !is_user_vaddr(vaddr) || !is_user_vaddr((const uint8_t*)vaddr+3)){
    printf("%s: exit(-1)\n", thread_current()->pcb->process_name);
    if(thread_current()->pcb->my_status != NULL)
      thread_current()->pcb->my_status->exit_status = -1;
    process_exit();
  }

  /* Check whether the vaddr is already mapped. If not , there'll be someting wrong when user accessed the vaddr*/
  struct thread* t =thread_current();
  if(t->pcb != NULL && t->pcb->pagedir != NULL){
    /*check the page of initial addr*/
    if(pagedir_get_page(t->pcb->pagedir,vaddr) == NULL){
      printf("%s: exit(-1)\n", thread_current()->pcb->process_name);
      if(t->pcb->my_status != NULL){
        t->pcb->my_status->exit_status = -1;
      }
      process_exit();
    }
  }
  
  /* check the page of tail addr*/
  const void *end_addr = (const uint8_t*)vaddr + 3;
  if(pg_round_down(vaddr) != pg_round_down((const uint8_t*)vaddr + 3));
  if(pagedir_get_page(t->pcb->pagedir,end_addr) == NULL){
    printf("%s: exit(-1)\n", thread_current()->pcb->process_name);
    if(t->pcb->my_status != NULL){
      t->pcb->my_status->exit_status = -1;
    }
    process_exit();
  }
  
}


struct lock filesys_lock;

void syscall_init(void) { 
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); 
  lock_init(&filesys_lock);
}

/* Function prototypes for file system-related syscalls */
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned size);
static int write (int fd, const void *buffer, unsigned size);
static void seek (int fd, unsigned position);
static int tell (int fd);
static int close (int fd);




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
    
    f->eax = process_fork(f);
  }

  /*File operator*/
  
  //create
  if(args[0] == SYS_CREATE){
    check_address((void*)args + 1);
    check_address((void*)args + 2);
    f->eax = create((const char*)args[1],(unsigned)args[2]);
    return;
  }

  //remove
  if(args[0] == SYS_REMOVE){
    check_address((void*)args + 1);
    f->eax = remove((const char*)args[1]);
    return;
  }

  //open
  if(args[0] == SYS_OPEN){
    check_address((void*)args+1);
    f->eax = open((const char*)args[1]);
    return;
  }

  //filesize
  if(args[0] == SYS_FILESIZE){
    check_address((void*)args+1);
    f->eax = filesize((int)args[1]);
    return;
  }

  //read
  if(args[0] == SYS_READ){
    check_address((void*)args+1);
    check_address((void*)args +2);
    check_address((void*)args+3);

    f->eax = read((int)args[1],(void*)args[2],(unsigned)args[3]);
    return;
  }

  //write
  if(args[0] == SYS_WRITE){
    check_address((void*)args+1);
    check_address((void*)args+2);
    check_address((void*)args+3);

    f->eax = write((int)args[1],(const void*)args[2],(unsigned)args[3]);
    return ;
  }

  //seek
  if(args[0] == SYS_SEEK){
    check_address((void*)args+1);
    check_address((void*)args+2);
    seek((int)args[1],(unsigned)args[2]);
    return;
  }

  //tell
  if(args[0] == SYS_TELL){
    check_address((void*)args+1);
    f->eax = tell((int)args[1]);
    return;
  }

  //close
  if(args[0] == SYS_CLOSE){
    check_address((void*)args+1);
    close((int)args[1]);
    return;
  }
  /*User thread*/

  /* Creates a new thread */
  if(args[0] == SYS_PT_CREATE){
    check_address((void*)args+1);
    check_address((void*)args+2);
    check_address((void*)args+3);
    f->eax = sys_pthread_create((stub_fun)args[1] , (pthread_fun)args[2] , (void*)args[3]);
    return;
  }

  /* Pthread join*/
  if(args[0] == SYS_PT_JOIN){
    check_address(args + 1);
    f->eax = sys_pthread_join((tid_t) args[1]);
    return;
  }

  /*Pthread exit*/
  if(args[0] == SYS_PT_EXIT){
    pthread_exit();
    NOT_REACHED();
  }

  /* User synchronization*/
  // seam syscall handler
  if(args[0] == SYS_SEMA_INIT){
    check_address((void*)args + 1);
    sys_sema_init((void*)args[1],args[2]);
    return;
  };
  if(args[0] == SYS_SEMA_DOWN){};
  if(args[0] == SYS_SEMA_UP){};
  
  //lock syscall handler
  if(args[0] == SYS_LOCK_INIT){};
  if(args[0] == SYS_LOCK_ACQUIRE){};
  if(args[0] == SYS_LOCK_RELEASE){};

  if(args[0] == SYS_GET_TID){};

  

}

/**
 * Creates a new file called file initially initial_size bytes in size. Returns true if successful,
 * false otherwise. Creating a new file does not open it: opening the new file is a separate
 * operation which would require an open system call.
 */
static bool create (const char *file, unsigned initial_size){
  check_address((void*)file);
  char *kfile = palloc_get_page(0);
  if (kfile == NULL) {
    return false;
  }
  strlcpy(kfile, file, PGSIZE);
  lock_acquire(&filesys_lock);
  bool success = filesys_create(kfile, (off_t) initial_size);
  lock_release(&filesys_lock);
  palloc_free_page(kfile);
  return success;
}

/**
 * Deletes the file named file. Returns true if successful, false otherwise. A file may be removed 
 * regardless of whether it is open or closed, and removing an open file does not close it. See 
 * this section of the FAQ for more details.
 */
static bool remove (const char *file){
  check_address((void*)file);
  char *kfile = palloc_get_page(0);
  if(kfile == NULL){
    return false;
  }
  strlcpy(kfile,file,PGSIZE);
  lock_acquire(&filesys_lock);
  bool success = filesys_remove(kfile);
  lock_release(&filesys_lock);
  palloc_free_page(kfile);
  return success;
}

/**
 * Opens the file named file. Returns a nonnegative integer handle called a “file descriptor” (fd), 
 * or -1 if the file could not be opened.
 */
static int open (const char *file){
  check_address((void*)file);
  char *kfile = palloc_get_page(0);
  if(kfile == NULL){
    return -1;
  }
  strlcpy(kfile,file,PGSIZE);

  lock_acquire(&filesys_lock);
  struct file *f = filesys_open(kfile);
  lock_release(&filesys_lock);
  palloc_free_page(kfile);

  if(f == NULL){
    return -1;
  }

  int fd = process_alloc_fd(f);
  
  if(fd == -1){
    lock_acquire(&filesys_lock);
    file_close(f);
    lock_release(&filesys_lock);
    return -1;
  }
  return fd;
}

/**
 * Returns the size, in bytes, of the open file with file descriptor fd. Returns -1 if fd does not 
 * correspond to an entry in the file descriptor table.
 */
static int filesize (int fd){
  struct file* file = process_get_file(fd);
  if(file == NULL)return -1;
  lock_acquire(&filesys_lock);
  int length = file_length(file);
  lock_release(&filesys_lock);
  return length;
}

/**
 * Reads size bytes from the file open as fd into buffer. Returns the number of bytes actually read 
 * (0 at end of file), or -1 if the file could not be read (due to a condition other than end of 
 * file, such as fd not corresponding to an entry in the file descriptor table). STDIN_FILENO reads 
 * from the keyboard using the input_getc function in devices/input.c.
 */

 static int read (int fd, void *buffer, unsigned size){
  if(fd == 0){
    unsigned i ;
    for(i =0;i<size;i++){
      check_address(buffer + i);
      uint8_t c = input_getc();
      if(c == '\0') break;
      *((uint8_t *)buffer + i ) = c; //buffer 原来的类型为 void* ,不允许直接进行指针运算 ； (uint8_t *)告诉编译器buffer是按字节寻址的指针，可以进行 + i运算。 最外层的 * 解引用出字符 c
    }
    return i;
  }

  struct file* f = process_get_file(fd);
  if(f == NULL)return -1;

  for(unsigned i =0;i<size ;i++){
    check_address(buffer + i);
  }

  char* kbuf = palloc_get_page(0);
  if(kbuf == NULL)return -1;

  off_t totla_read = 0;
  while(totla_read < (off_t)size){
    off_t to_read = size - totla_read;
    if(to_read >PGSIZE)to_read = PGSIZE;
      lock_acquire(&filesys_lock);
      off_t n = file_read(f, kbuf,to_read);
      lock_release(&filesys_lock);
      if(n<=0) break;

      memcpy(buffer + totla_read,kbuf,n);
      totla_read +=n;
      if(n<to_read) break;
  }
  palloc_free_page(kbuf);
  return totla_read;
}

 /**
  * Writes size bytes from buffer to the open file with file descriptor fd. Returns the number of 
  * bytes actually written, which may be less than size if some bytes could not be written. Returns 
  * -1 if fd does not correspond to an entry in the file descriptor table.
  */
 static int write (int fd, const void *buffer, unsigned size){
  if(fd == 0){
    return -1;
  }

  if(fd == 1){
    for(unsigned i =0; i<size;i++){
      check_address(buffer + i);
    }
    putbuf((const char*)buffer,size);
    return size;
  }

  for(unsigned i =0; i<size;i++){
    check_address(buffer + i);
  }

  struct file* f =process_get_file(fd);
  if(f == NULL)return -1;

  char* kbuf = palloc_get_page(0);
  if(kbuf == NULL)return -1;

  off_t total_written =0;
  while(total_written < (off_t)size){
    off_t to_write = size - total_written;
    if(to_write >PGSIZE)to_write = PGSIZE;

    memcpy(kbuf,buffer+total_written,to_write);

    lock_acquire(&filesys_lock);
    off_t n = file_write(f,kbuf,to_write);
    lock_release(&filesys_lock);
    if(n<=0) break;

    total_written += n;
    if(n<to_write)break;
  }

  palloc_free_page(kbuf);
  return total_written;
 }

 /**
  * Changes the next byte to be read or written in open file fd to position, expressed in bytes 
  * from the beginning of the file. Thus, a position of 0 is the file’s start. If fd does not 
  * correspond to an entry in the file descriptor table, this function should do nothing.
  */
 static void seek (int fd, unsigned position){
  if (fd == 0 || fd == 1 )return ;
  struct file* f = process_get_file(fd);
  if(f == NULL)return ;
  lock_acquire(&filesys_lock);
  file_seek(f,position);
  lock_release(&filesys_lock);
  return;
 }

 /**
  * Returns the position of the next byte to be read or written in open file fd, expressed in bytes 
  * from the beginning of the file. If the operation is unsuccessful, it can either exit with -1 or 
  * it can just fail silently.
  */
 static int tell(int fd){
  if(fd ==0 || fd ==1)return -1;

  struct file* f = process_get_file(fd);
  if(f == NULL)return -1;

  lock_acquire(&filesys_lock);
  int pos = file_tell(f);
  lock_release(&filesys_lock);
  return pos;
 }

 /**
  * Closes file descriptor fd. Exiting or terminating a process must implicitly close all its open 
  * file descriptors, as if by calling this function for each one. If the operation is 
  * unsuccessful, it can either exit with -1 or it can just fail silently.
  */

  static int close (int fd){
    struct thread* t = thread_current();
    struct list_elem* e;
    struct fd_entry* entry = NULL;
    
    for(e = list_begin(&t->pcb->fds); e != list_end(&t->pcb->fds); e = list_next(e)){
      struct fd_entry* fe = list_entry(e, struct fd_entry, elem);
      if(fe->fd == fd){
        entry = fe;
        break;
      }
    }
    if(entry == NULL) return -1;
    
    lock_acquire(&filesys_lock);
    file_close(entry->file);
    lock_release(&filesys_lock);
    list_remove(&entry->elem);
    free(entry);
    return 0;
  }