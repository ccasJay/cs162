### Well-known Ports
| 端口    | 协议      | 主要用途        |     |
| ----- | ------- | ----------- | --- |
| 20    | TCP     | FTP 数据传输    |     |
| 21    | TCP     | FTP 控制      |     |
| 22    | TCP     | SSH（远程登录）   |     |
| 23    | TCP     | Telnet      |     |
| 25    | TCP     | SMTP（邮件发送）  |     |
| 53    | UDP/TCP | DNS（域名解析）   |     |
| 80    | TCP     | HTTP（网页）    |     |
| 110   | TCP     | POP3（邮件接收）  |     |
| 143   | TCP     | IMAP（邮件接收）  |     |
| 443   | TCP     | HTTPS（加密网页） |     |
| 3306  | TCP     | MySQL       |     |
| 5432  | TCP     | PostgreSQL  |     |
| 6379  | TCP     | Redis       |     |
| 27017 | TCP     | MongoDB     |     |
![[Pasted image 20260404124322.png]]
![[Pasted image 20260404125341.png]]
![[Pasted image 20260404130017.png]]
- Concurrent Server without Protection
	- 为每个连接生成一个新的线程
	- 主线程初始化新的客户端连接不需要等待先前生成的线程
	- 这样做提高了新建线程和线程间切换的效率
	![[Pasted image 20260404131640.png]]
- 如何理解 __FIle Descriptors__: 
	- 是一个**非负整数**, 是操作系统为了管理某个被进程打开的各种“文件对象“ ，而返回给进程的一个号码牌
	- 进程私有
	- 分配原则: 永远分配当前可用的最小数字
	- 结束后 用`close (fd)`回收
	- fd存在进程级上限，unix系统中 用`ulimit -n` 查看

### **pthreads API 简单解释**（POSIX 标准线程库，最常用的 C 语言线程接口）：

 1. **pthread_create**（最重要）
```c
int pthread_create(pthread_t *thread, ... , void *(*start_routine)(void*), void *arg);
```
- **作用**：创建一个新线程，让它立刻去执行 `start_routine` 函数。
- 参数 `arg` 是传给新线程的唯一参数。
- 创建成功后，线程自动开始运行，**不需要手动调用 pthread_exit**，函数结束就相当于退出了。

 2. **pthread_exit**
```c
void pthread_exit(void *value_ptr);
```
- **作用**：主动结束当前线程，并把 `value_ptr`（返回值）交给其他线程（通过 join 接收）。

 3. **pthread_yield**
```c
int pthread_yield();
```
- **作用**：当前线程主动让出 CPU，让其他线程先跑（类似“礼让”）。

 4. **pthread_join**（等待线程结束）
```c
int pthread_join(pthread_t thread, void **value_ptr);
```
- **作用**：当前线程**阻塞**，一直等到指定的 `thread` 结束。
- 如果 `value_ptr` 不为 NULL，就能拿到对方 `pthread_exit` 传回来的值。

**一句话总结**：  
`pthread_create` 生孩子，`pthread_join` 等孩子死，`pthread_exit` 是孩子自己说“我死了”，`pthread_yield` 是孩子说“你先玩”。

这就是 pthreads 最核心的 4 个函数。