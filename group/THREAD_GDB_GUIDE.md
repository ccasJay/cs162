# Pintos 线程机制 GDB 调试指南

这份文档的目标不是单纯教你“怎么下断点”，而是帮助你通过 GDB 真正理解 Pintos 中线程的创建、调度和上下文切换机制。

---

## 1. 这次调试要弄明白什么

建议你带着下面 4 个问题去调试：

1. 线程是怎么创建出来的？
2. 线程什么时候进入 `ready`、`running`、`blocked` 状态？
3. `schedule()` 怎么决定下一个运行哪个线程？
4. 为什么线程 A 调用 `switch_threads()`，返回时却可能已经是线程 B？

最关键的是第 4 个问题。

---

## 2. 重点源码位置

建议先把这些函数和文件打开：

- `src/threads/thread.c`
- `src/threads/thread.h`
- `src/threads/switch.S`

本次调试重点关注：

- `thread_create()`
- `thread_unblock()`
- `thread_yield()`
- `thread_block()`
- `schedule()`
- `next_thread_to_run()`
- `thread_switch_tail()`
- `switch_threads`
- `switch_entry`
- `kernel_thread()`

对应位置：

- [thread_create()](src/threads/thread.c#L176-L212)
- [thread_unblock()](src/threads/thread.c#L251-L261)
- [thread_yield()](src/threads/thread.c#L303-L315)
- [thread_block()](src/threads/thread.c#L221-L227)
- [schedule()](src/threads/thread.c#L538-L550)
- [next_thread_to_run()](src/threads/thread.c#L484-L486)
- [thread_switch_tail()](src/threads/thread.c#L504-L529)
- [switch_threads](src/threads/switch.S#L17-L48)
- [switch_entry](src/threads/switch.S#L51-L64)
- [kernel_thread()](src/threads/thread.c#L394-L399)

---

## 3. 启动调试

### 方式一：使用仓库里的脚本

如果你在项目根目录：

```bash
./pgdb.sh
```

如果你在 `src/threads/build`：

```bash
../../pgdb.sh
```

默认会启动一个适合线程项目的测试，并自动进入 gdb。

### 方式二：手动启动

在 `src/threads/build` 下：

#### 终端 1

```bash
pintos --gdb -- run alarm-multiple
```

#### 终端 2

```bash
pintos-gdb kernel.o
```

进入 gdb 后执行：

```gdb
debugpintos
```

---

## 4. 第一轮：观察线程创建

### 建议断点

```gdb
b thread_create
b thread_unblock
b kernel_thread
c
```

### 在 `thread_create()` 中看什么

停在 [thread_create()](src/threads/thread.c#L176-L212) 后：

```gdb
bt
n
n
p name
p priority
p function
p aux
```

这里重点看：

- `name`：线程名字
- `priority`：优先级
- `function`：线程第一次真正执行的函数
- `aux`：线程函数的参数

### 查看新线程结构体

在 `init_thread(t, ...)` 之后：

```gdb
p t
p *t
```

重点观察：

- `t->status`
- `t->name`
- `t->stack`
- `t->priority`

你会看到：新线程一开始不是 `THREAD_RUNNING`，而是先被初始化为阻塞态，之后才进入 ready 队列。

### 观察伪造的初始栈帧

继续往下走到：

- `kf = alloc_frame(...)`
- `ef = alloc_frame(...)`
- `sf = alloc_frame(...)`

然后看：

```gdb
p kf
p ef
p sf
p t->stack
```

这一步的核心理解：

**线程第一次运行之前，Pintos 已经帮它把栈伪造好了。**

它不是“从空白状态开始跑”，而是被安排成一个“马上就能从调度切换现场恢复执行”的样子。

### 看它怎么进入 ready 状态

走到 `thread_unblock(t);` 时：

```gdb
s
```

进入 [thread_unblock()](src/threads/thread.c#L251-L261)：

```gdb
p *t
n
n
n
p t->status
```

你会看到它从 `THREAD_BLOCKED` 变成 `THREAD_READY`。

---

## 5. 第二轮：观察 yield / block 如何触发调度

### 建议断点

```gdb
b thread_yield
b thread_block
b schedule
c
```

### 如果停在 `thread_yield()`

在 [thread_yield()](src/threads/thread.c#L303-L315)：

```gdb
bt
p *thread_current()
```

再向下执行：

```gdb
n
n
p cur
p *cur
```

你会看到它最终执行：

```c
cur->status = THREAD_READY;
schedule();
```

这说明：

**当前线程不是被销毁，而是主动让出 CPU，并把自己重新放回 ready 队列。**

### 如果停在 `thread_block()`

在 [thread_block()](src/threads/thread.c#L221-L227)：

```gdb
bt
p *thread_current()
n
p thread_current()->status
```

这里当前线程会变成 `THREAD_BLOCKED`，然后调用 `schedule()`。

---

## 6. 第三轮：重点观察 `schedule()`

### 建议断点

```gdb
b schedule
c
```

到 [schedule()](src/threads/thread.c#L538-L550) 后：

```gdb
n
n
p cur
p next
p *cur
p *next
```

如果变量还没赋值完成，就再多 `n` 几步。

这里重点看：

- `cur` 是谁
- `next` 是谁
- `cur->status` 是否已经不是 `THREAD_RUNNING`
- `next->status` 是否是 `THREAD_READY`

这一步帮助你理解：

**调度器真正做的事情，就是从 ready 集合中选一个线程出来运行。**

### 想进一步看 ready 队列

可以继续在下面两个位置打断点：

```gdb
b next_thread_to_run
b thread_schedule_fifo
c
```

在 [thread_schedule_fifo()](src/threads/thread.c#L451-L456) 中，你会看到：

- 队列非空：`list_pop_front(...)`
- 队列为空：返回 `idle_thread`

---

## 7. 第四轮：真正看上下文切换 `switch_threads`

这是最关键的一轮。

### 建议断点

```gdb
b switch_threads
c
```

停在 [switch_threads](src/threads/switch.S#L17-L48) 后，不要用 `next`，要用汇编单步：

```gdb
display/i $pc
info registers
si
si
si
```

### 第一步：保存当前线程的栈指针

在 [switch.S:35-37](src/threads/switch.S#L35-L37)：

```asm
movl SWITCH_CUR(%esp), %eax
movl %esp, (%eax,%edx,1)
```

这表示：

- 取出参数 `cur`
- 把当前 CPU 的 `%esp` 保存到 `cur->stack`

可以观察：

```gdb
info registers esp eax edx
```

### 第二步：切换到下一个线程的栈

在 [switch.S:39-41](src/threads/switch.S#L39-L41)：

```asm
movl SWITCH_NEXT(%esp), %ecx
movl (%ecx,%edx,1), %esp
```

这一步最关键。

它表示：

- 取出参数 `next`
- 读取 `next->stack`
- 直接把 CPU 的 `%esp` 改成它

你可以在前后分别看：

```gdb
info registers esp
si
info registers esp
```

你会看到：

**CPU 的栈已经从当前线程切到另一个线程了。**

### 第三步：理解 `ret` 为什么会回到另一个线程

继续单步到 `ret`：

```gdb
si
bt
info registers esp eip
```

你会发现：

- 进入 `switch_threads` 的明明是线程 A
- 但 `ret` 之后的调用栈，已经像线程 B 自己一路执行到这里一样

这就是线程切换最反直觉但最核心的点。

**调用 `switch_threads()` 的线程和从 `switch_threads()` 返回的线程，可能不是同一个线程。**

原因是：

- `ret` 依赖当前 `%esp` 指向的栈内容
- `%esp` 已经被换成了 `next->stack`
- 所以它会沿着新线程自己的栈返回

---

## 8. 第五轮：观察新线程第一次运行

新线程第一次运行时，并不是直接跳进你的线程函数，而是会先经过一个过渡流程。

### 建议断点

```gdb
b switch_entry
b thread_switch_tail
b kernel_thread
c
```

### `switch_entry`

到 [switch_entry](src/threads/switch.S#L51-L64) 时：

```gdb
display/i $pc
si
si
si
```

它会做三件事：

1. 丢掉 `switch_threads()` 的参数
2. 调用 `thread_switch_tail(prev)`
3. 再 `ret` 到后续入口

### `thread_switch_tail()`

在 [thread_switch_tail()](src/threads/thread.c#L504-L529)：

```gdb
p prev
p *running_thread()
n
p running_thread()->status
```

这里会把当前线程状态设成：

- `THREAD_RUNNING`

同时开始新的时间片。

### `kernel_thread()`

到 [kernel_thread()](src/threads/thread.c#L394-L399) 时：

```gdb
bt
p function
p aux
```

它会先：

```c
intr_enable();
```

然后真正执行：

```c
function(aux);
```

这就是线程第一次“正式开始跑”。

---

## 9. 你最终应该得出的结论

### 1. 每个线程都有自己独立的内核栈

看 [thread.h:30-34](src/threads/thread.h#L30-L34) 的说明：

- `struct thread` 在页底部
- 内核栈在页顶部向下增长

因此线程切换的本质，就是**切换栈**。

### 2. `cur->stack` 保存的是线程被切走时的栈顶

在 [switch.S:35-37](src/threads/switch.S#L35-L37) 保存。

这使得该线程将来还能从同一个执行点继续恢复。

### 3. `next->stack` 决定了线程恢复到哪里

在 [switch.S:39-41](src/threads/switch.S#L39-L41) 恢复。

只要 `%esp` 被替换成 `next->stack`，CPU 的执行上下文就已经切到下一个线程。

### 4. `switch_threads()` 的调用者和返回者可能不是同一个线程

这是 Pintos 线程机制里最关键的一点。

- 线程 A 调用 `switch_threads(cur, next)`
- 但返回时可能已经是线程 B

这是因为 `ret` 用的是新线程栈上的返回地址。

### 5. 新线程第一次运行依赖伪造好的初始栈帧

在 [thread_create()](src/threads/thread.c#L194-L208) 中，Pintos 已经提前布置好了：

- `kernel_thread_frame`
- `switch_entry_frame`
- `switch_threads_frame`

所以新线程虽然从未真正运行过，却能像普通线程一样被调度并恢复执行。

---

## 10. 最常用的 GDB 命令

如果你不想一开始记太多，最实用的一组命令是：

```gdb
b thread_create
b thread_unblock
b thread_yield
b thread_block
b schedule
b switch_threads
b switch_entry
b thread_switch_tail
b kernel_thread
c
```

停住后常用：

```gdb
bt
n
s
si
p cur
p next
p *cur
p *next
p *thread_current()
info registers
display/i $pc
x/20x $esp
```

---

## 11. 最推荐的观察顺序

建议按下面顺序来，最容易理解：

### 第一遍
只看：

- `thread_create`
- `thread_unblock`

目标：明白线程是怎么出生、怎么进 ready 的。

### 第二遍
只看：

- `thread_yield`
- `thread_block`
- `schedule`

目标：明白调度是什么时候发生的。

### 第三遍
只看：

- `switch_threads`

目标：明白上下文切换的本质是换栈。

### 第四遍
只看：

- `switch_entry`
- `thread_switch_tail`
- `kernel_thread`

目标：明白新线程第一次为什么也能正常开始执行。

---

## 12. 一句话总结

如果你最后只记住一句话，那就是：

**Pintos 的线程切换本质上不是“函数切换”，而是“内核栈切换”；谁的栈被装进 `%esp`，CPU 就继续沿着谁的执行现场跑下去。**
