#include <stdio.h>
#include <sys/resource.h>

int main() {
    struct rlimit lim;
    if(getrlimit(RLIMIT_STACK,&lim)==0){
        printf("stack size: %ld\n", (long)lim.rlim_cur);
    }
    if(getrlimit(RLIMIT_NPROC,&lim)==0){
        printf("process limit: %ld\n", (long)lim.rlim_cur);
    }
    if(getrlimit(RLIMIT_NOFILE,&lim)==0){
        printf("max file descriptors: %ld\n", (long)lim.rlim_cur);
    }
    return 0;
}
