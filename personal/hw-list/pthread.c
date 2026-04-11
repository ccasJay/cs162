/*
 * You may NOT modify this file. Any changes you make to this file will not
 * be used when grading your submission.
 */

 /*
Read pthread.c carefully. Then, run make and run pthread multiple times and observe its output. Answer the following questions based on your observations.

1. Is the program’s output the same each time it is run? Why or why not?
=> Not the same. 因为每个新生成的thread的stack地址和threadid都是不同的，所以每次运行程序时，输出都会有所不同。

2. Based on the program’s output, do multiple threads share the same stack?
=> No. 每个线程都有自己的stack地址，所以多个线程不共享同一个stack。
3. Based on the program’s output, do multiple threads share the same global variables?
=> Yes, 所有线程共享同一个global variable common，因为它们的地址是相同的，并且在输出中显示了相同的地址和值。
4. Based on the program’s output, what is the value of void *threadid? How does this relate to the variable’s type (void *)?
=> Thread id like #1,#2,etc
5. Using the first command line argument, create a large number of threads in pthread. Do all threads run before the program exits? Why or why not? Note: Please be precise. Vague responses will not be given credit.
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>  
#include <string.h>

#define NUM_THREADS 4
int common = 162;
char* somethingshared;
// threadfun()是每个线程执行的函数，它接受一个void*类型的参数threadid，表示线程的ID。函数内部打印了线程的stack地址、common变量的地址和值，以及threadid的值和somethingshared字符串的一部分。最后，线程调用pthread_exit()退出。
void* threadfun(void* threadid) {
  long tid;
  tid = (long)threadid;
  printf("Thread #%lx stack: %lx common: %lx (%d) tptr: %lx\n", tid, (unsigned long)&tid,
         (unsigned long)&common, common++, (unsigned long)threadid);
  printf("%lx: %s\n", (unsigned long)somethingshared, somethingshared + tid);
  pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
  int rc;
  long t;
  int nthreads = NUM_THREADS;
  char* targs = strcpy(malloc(100), "I am on the heap.");

  if (argc > 1) {
    nthreads = atoi(argv[1]);
  }

  pthread_t threads[nthreads];

  printf("Main stack: %lx, common: %lx (%d)\n", (unsigned long)&t, (unsigned long)&common, common);
  puts(targs);
  somethingshared = targs;
  for (t = 0; t < nthreads; t++) {
    printf("main: creating thread %ld\n", t);
    rc = pthread_create(&threads[t], NULL, threadfun, (void*)t);
    if (rc) {
      printf("ERROR; return code from pthread_create() is %d\n", rc);
      exit(-1);
    }
  }

  /* Last thing that main() should do */
  pthread_exit(NULL);
}
