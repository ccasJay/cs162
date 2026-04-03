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
    - SIGKILL, SIGSTOP : 由系统发送的强制终止和停止信号，无法被捕获或忽略，用于立即终止或暂停程序的执行.


### Unix/POSIX idea: Everything is a "File"
- 系统中的各个接口都是抽象成了文件，底层其实就是**字节流**
- `ioctl()`: 一个系统调用，用于无法通过read, write等操作完成的特定设备控制任务
-  Streams:
	```cpp
	#include<stdio.h>
	FILE *fopen( const char *filename, const char *mode);
	int fclose( FILE *fp);
	```
	

[[文件操作mode]]

[[C API Standard Streams]] 

![[Pasted image 20260403205732.png]]
![[Pasted image 20260403210514.png]]

### The Socket Abstraction
- 把网络通信伪装成读写文件

### Socket Creation
- **FIle system**:文件系统提供一些固定的objects在name space中
	- 进程读写开关这些对象
	- 文件独立于进程存在
	- 便于命名文件
- **Pipes** : 用于进程之间连接
	- 单队列
	- 临时通过 `pipe()` 调用
	-  父传子
- **Sockets**: 同设备或不同设备间建立沟通
	- 双队列
	- 进程无祖先，可以从设备中分离