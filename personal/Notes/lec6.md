# Synchronization 1: Concurrency and Mutual Exclusion

### The core  of concurrency : the Dispatch Loop
- how to run a thread: 
	1. load the state(寄存器，PC，stack pointer)
	2. load the environment (虚拟内存空间)
	3. jump the pc
-  *Dispatcher (调度分派器)* :
	- 紧跟在 scheduler后
	- **上下文切换（Context Switch）：** 保存当前正在运行进程的状态（寄存器、程序计数器 PC、堆栈指针等），并加载下一个进程之前保存的状态。
	- **切换到用户模式：** 确保 CPU 从内核态切换回用户态，以便程序运行。
	- **跳转到正确位置：** 跳转到用户程序的适当位置，重新启动该程序。

- *Internal events*
	- Blocking on I/O
	- signal from other thread
	-  yield()

- *External Events*
	- interrupts : signal from hardware or software that stop the running code and jump to kernel
	- timer

![[Pasted image 20260408164107.png]]
![[Pasted image 20260408164415.png]]

- `ThreadFork(fcnPtr, fcnArgPtr, size of stack to allocate)`
	- implement:
		1. Sanity check arg
		2. enter kernel-mode and sanity check arg again
		3. allocate new stack and TCB
		4. size of stack to allocate

### Initialize TCB and Stack
- Initialize Register fields of TCB: 
	- stack pointer
	- PC return address 
	- two arg resister , fcnPtr and fcnArgPtr

### How does a thread get started
- 在threads进入ready queue时，不管它之前是否 is running，都要伪造成is running
- how to? -=>
	1. 申请TCB and thread stack 的空间
	2.  `sp` 指向 the top of new stack; `pc(返回地址)` 设置为`ThreadRoot`函数的地址
	3. 存入 `fcnPtr`  , `fcnArgPtr`
	4. put into the Ready Queue

### Multiprocessing vs Multiprogramming vs Multithreading
![[Pasted image 20260408174841.png]]
![[Pasted image 20260408175026.png]]






---
llm 
- how does it know to put the context to the thread stack instead of the kernel stack: 
这是一个非常深刻的问题，触及了操作系统内核设计的核心。在 [UC Berkeley CS162](https://www.bilibili.com/video/BV1L541117gr?vd_source=e293470ea109e008c4d9516e39ef318f&spm_id_from=333.788.videopod.episodes&p=6) 的语境下，特别是针对 **Pintos** 这种教学操作系统，答案可以分为以下三个层次来理解：

### 1. 核心误区澄清：这里的 Thread Stack “就是” Kernel Stack
在讨论内核里的 `switch_threads`（分派器）时，**Thread Stack 和 Kernel Stack 其实是同一个东西**。
- 对于每一个内核线程（Kernel Thread），它只有一个栈，即 **Kernel Stack**。
- 对于用户进程的线程，它有两个栈：**User Stack**（跑用户代码用）和 **Kernel Stack**（进内核办事用）。
- **关键点：** 所有的“切换”动作（Context Switch）都是在内核态完成的。因此，`switch()` 函数保存现场时，它面对的永远是当前线程的 **Kernel Stack**。
### 2. 它怎么知道存到哪？—— TCB 是导航仪
系统并不需要“猜”存到哪里，因为 `switch(tCur, tNext)` 函数的参数直接指明了方向。
在视频中展示的 `SetupNewThread` 逻辑里：
1. 每个线程都有一个 **TCB（Thread Control Block）**。
2. TCB 里有一个关键字段叫做 `stack_pointer` (或 `regs.sp`)。
3. **保存：** 当执行 `switch` 时，汇编代码会把当前的 CPU 寄存器压入当前的栈，然后把此时的栈指针（%esp）存入 `tCur->stack_pointer`。
4. **恢复：** 接着，它把 `tNext->stack_pointer` 的值加载到 CPU 的 %esp 寄存器。
5. **结果：** 这一瞬间，CPU 就已经切换到了新线程的栈顶。
### 3. 如果是用户态线程，Context 存哪了？
这是最容易搞混的地方。一个用户线程进入内核（通过系统调用或中断）时，会经历**两次**保存：
- **第一步（进入内核）：** CPU 硬件或底层中断处理程序会自动把用户态的寄存器（用户栈指针、PC 等）压入该线程的 **Kernel Stack** 中。
- **第二步（线程切换）：** 当内核决定切换线程时，调用 `switch()`，它会把**内核态**的寄存器（刚才运行内核代码的状态）再次压入同一个 **Kernel Stack**。
### 简洁总结
- **没有“二选一”：** 切换发生在内核中，所以一定存入 **Kernel Stack**。
- **指针引导：** 每个线程的 **TCB** 记录了它自己栈的位置。
- **自包含：** 栈指针（%esp）就像一个书签，`switch` 函数只是把旧书签收起来贴在旧 TCB 上，然后从新 TCB 上拿起新书签夹在 CPU 里。
你在看视频时，是不是正疑惑为什么 `switch` 只有几行汇编代码就能完成这么复杂的操作？其实秘密全在于那个 `%esp` 寄存器的指向转换。

针对这部分，你是在理解 `switch_threads` 的汇编实现上有困难，还是对用户态/内核态的转换更感兴趣？
