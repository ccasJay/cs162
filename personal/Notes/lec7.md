- How does the hardware know the address of the kernel stack?
	通过TSS结构来获取kernel stack

- Timer trigger thread switch 
	- `thread_tick`
	- `thread_yield`
	- Schedule

**![[Pasted image 20260411233255.png]]**
### 1. 硬件触发 (Hardware interrupt vector)

- **左侧的表**：当硬件（比如主板上的定时器芯片）发出中断信号时，CPU 会根据中断号（这里是 `0x20`，十进制的 32），去查找**硬件中断向量表**。
    
- 这个表里存放着对应中断的入口地址。CPU 会根据这个地址跳转过去。
    

### 2. 专属汇编入口 (intrNN_stub)

- 跳转过来后，执行的是一小段专门为 `0x20` 准备的汇编代码（stub）。
    
- **动作**：它干了两件事。第一，`push 0x20`，把自己的中断号压入栈中（这样后面的代码才知道是哪个中断发生了）；第二，`jmp intr_entry`，立刻跳转到一个**通用**的处理入口。
    

### 3. 通用汇编包装器 (stubs.S - intr_entry)

- 所有不同的硬件中断，最后都会汇聚到这个叫 `intr_entry` 的地方。
    
- **动作**：
    
    - `save regs as frame`：**这是最关键的一步！**它会把 CPU 当前的所有寄存器状态（也就是被打断的用户程序的状态）一股脑地压入内核栈中，打包成一个结构体，叫做 **中断帧 (frame)**。
        
    - `set up kernel env`：配置好 C 语言运行所需的内核环境。
        
    - `call intr_handler`：调用 C 语言写的主处理函数，并将刚才打包好的 `frame` 的指针传给它。
        

### 4. C 语言通用调度器 (interrupt.c - intr_handler)

- 现在进入了高级语言（C语言）的世界。`intr_handler` 就像一个**交通警察**。
    
- **动作**：
    
    - `classify` & `dispatch`：它查看传入的 `frame` 里记录的中断号（之前压入的 `0x20`），然后去右下角的**软件中断处理函数表 (Pintos intr_handlers)** 中查表，找到真正负责处理这个中断的 C 函数。
        
    - `maybe thread yield`：处理完后，它可能会决定当前线程时间片用完了，强制让出 CPU。
        

### 5. 真正的处理逻辑 (timer.c - timer_intr)

- 交通警察把你引导到了具体的工位。因为是 `0x20`，所以调用的是 `timer_intr`（时钟中断处理程序）。
    
- **动作**：在这里执行真正的业务逻辑，比如 `tick++`（系统时间滴答加一），然后调用 `thread_tick()` 检查当前线程是不是该被调度走了。
    

### 6. 原路返回 (stubs.S - intr_exit)

- 等 C 函数全部执行完毕后，代码流会返回到汇编文件里的 `intr_exit` 标签处。
    
- **动作**：
    
    - `restore regs`：把步骤 3 中保存在栈里的那个 `frame` 拆开，把寄存器的值一一恢复到 CPU 硬件中。
        
    - `iret`：执行**中断返回指令**（这就是我们上一张图提到的那个机制），CPU 特权级降级，程序指针跳回到最初被打断的用户程序继续执行。                  

### Atomic Operation
- It's indivisible
- it's fundamental building block

- __Synchronization__ : using atomic operations to ensure cooperation between threads
- __Mutual Exclusion__ : ensuring that only one thread does a particular thing at a time 
- __Critical Section__ : piece of code that only one thread can execute at onec.Only one thread at a time will get into this section of code.

### Locks 
 - `acquire(&lock)` 
 - `release(&lock`

- __Spurious Wakeup__:
	
	**虚假唤醒（Spurious Wakeup）**是指在多线程编程中，**即使没有任何其他线程调用** **signal** **或** **broadcast** **来发送唤醒信号，等待在条件变量上的线程也可能会自己醒来并从** **wait** **调用中返回**。
	
	这意味着当线程醒来时，它所等待的状态条件可能不仅现在不成立，甚至可能从未成立过。这种现象在 Java 和 POSIX pthreads 的底层平台实现中通常被允许，作为对底层平台语义的一种让步。
	
	为了应对虚假唤醒，编程的标准规范是：**必须始终将** **wait** **调用放在一个** **while** **循环中，并在循环条件中测试程序所等待的状态谓词**。这样一来，即使发生了虚假唤醒，线程也会在循环中重新检查条件，如果条件并未满足，线程将继续挂起等待，从而避免了程序逻辑错误。

- __Three pitfalls__;
	**1. 双重检查锁定（Double-Checked Locking）** 许多程序员为了所谓的“性能优化”，会试图在不必要时省略获取锁的操作。最典型的例子是单例模式的延迟初始化：为了避免每次都加锁，程序先在锁外部检查实例指针是否为空，如果为空再获取锁并进行第二次检查和初始化。 然而，这是一种非常危险的代码模式，因为诸如 `pInstance = new Instance()` 的赋值语句并不是原子的。它底层包含了三个步骤：分配内存、运行构造函数初始化对象、将指针指向新分配的内存。现代编译器和处理器硬件常常会对这些指令进行重排（Reordering），这意味着可能会先执行指针赋值，再执行初始化。如果发生这种情况，另一个线程可能会在外部检查时看到一个非空的指针，并在对象尚未完全初始化时就开始使用它，从而导致程序崩溃或不可预知的错误。 **核心建议**：不要尝试通过省略锁来进行此类风险极高的“优化”。应养成简单的编程习惯——访问共享变量时始终在外部加锁，除非详尽的性能分析证明这里存在严重的瓶颈且必须使用此优化。
	
	**2. 避免在方法中间定义同步块（Avoid defining a synchronized block in the middle of a method）** Java 语言提供了一个便利的功能：可以在方法的任意中间位置使用 `synchronized{ ... }` 关键字来将某一段代码定义为同步块。 然而，这种结构违反了“始终在方法的最开始获取锁，在返回前释放锁”的并发编程最佳实践。将同步块深埋在方法中间，会极大地破坏代码的结构，让阅读代码的人难以迅速辨别哪些状态被锁保护。 **核心建议**：当你发现自己想要在 Java 方法的中间写一个 `synchronized` 块时，请将其视为一个强烈的信号：你应该把这部分需要同步的代码逻辑提取出来，重构成一个独立的新方法。
	
	**3. 保持共享状态类与线程类分离（Keep shared state classes separate from thread classes）** 在 Java 中，编写线程逻辑通常需要继承 `Thread` 类或实现 `Runnable` 接口。程序员经常犯的错误是，在编写继承了 `Thread` 的类时，不仅把线程的主循环（Main Loop）写在里面，还把供多个线程访问的“共享状态”也塞在同一个类里（比如把“共享队列”和“工作线程”混为一个类）。 这种做法模糊了“线程（执行流）”和“共享对象（数据与状态）”之间的界限，会造成极其混乱的代码逻辑。 **核心建议**：必须严格分离线程和共享对象。任何继承了 `Thread` 或实现了 `Runnable` 的类中，绝对不应该包含被多线程共享的状态、锁以及条件变量