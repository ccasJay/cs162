## lec4
### Starting new Program: variants of exec
```cpp
cpid = fork();
if(cpid > 0){
    tcpid = wait(&status); //tcpid is the pid of the child process that has terminated, and status is a pointer to an integer where the exit status of the child process will be stored.
}else if(cpid ==0){
    char *args[]={"ls","-1",NULL};
    execv("/bin/ls", args); //v means vector, the args are passed as an array of strings
    /* execv doesn't return when it works */
    peeror("execv"); // If execv returns, it means it failed, so we print an error message and exit with a non-zero status to indicate failure.
    exit(1);
}
```

### Common POSIX Signals(常见的POSIX信号)
- POSIX: Portable Operating System Interface for Unix，是一套由IEEE制定的操作系统接口标准，旨在提高不同Unix系统之间的兼容性和可移植性。
    - SIGINT : 由用户发送的中断信号，通常通过按下Ctrl+C触发，用于请求程序终止。
    - SIGTERM : 由系统或其他程序发送的终止信号，通常
    - SIGSTP : 由用户发送的停止信号，通常通过按下Ctrl+Z触发，用于暂停程序的执行。
    - SIGKILL, SIGSTOP : 由系统发送的强制终止和停止信号，无法被捕获或忽略，用于立即终止或暂停程序的执行。
