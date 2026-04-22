# Pintos 项目概览

Pintos 是一个用于 80x86 架构的简单操作系统框架。它专为教学本科操作系统课程而设计，重点关注内核核心概念，如线程、用户程序、虚拟内存和文件系统。

## 项目结构

- **`src/threads/`**: 内核核心线程、调度和同步原语。
- **`src/userprog/`**: 用户程序加载与执行、系统调用处理及进程管理。
- **`src/vm/`**: 虚拟内存管理，包括分页、补充页表和交换（swapping）。
- **`src/filesys/`**: 文件系统实现（初始为基础版，通常在项目期间进行扩展）。
- **`src/devices/`**: 硬件设备驱动程序（定时器、键盘、磁盘、串口等）。
- **`src/lib/`**: 适用于内核 (`lib/kernel/`) 和用户程序 (`lib/user/`) 的标准 C 库。
- **`src/utils/`**: 用于在宿主机上运行和调试 Pintos 的实用程序（例如 `pintos` 脚本）。
- **`src/tests/`**: 涵盖所有项目阶段的自动化测试套件。
- **`src/examples/`**: 示例用户程序。

## 构建与运行

### 前置条件
如果宿主机不是 x86 架构，Pintos 需要 32 位 x86 交叉编译器（如 `i686-linux-gnu-gcc`）。它使用 **QEMU** 或 **Bochs** 等模拟器运行。

### 构建
在任一项目子目录（如 `threads`、`userprog`、`vm`、`filesys`）中构建内核：

```bash
cd src/threads
make
```

这将创建一个包含 `kernel.bin` 和 `loader.bin` 的 `build` 目录。

### 运行
使用位于 `src/utils/` 的 `pintos` 脚本。确保 `src/utils` 已添加到你的 `PATH` 中。

```bash
# 在 threads 目录中运行 'alarm-multiple' 测试
pintos --qemu -- -q run alarm-multiple

# 运行带有文件系统的用户程序 (userprog)
pintos --qemu --filesys-size=2 -p ../examples/echo -a echo -- -q run 'echo hello'
```

### 测试
在相关项目的 `build` 目录中运行测试：

```bash
cd src/threads/build
make check
```

或者运行评分脚本：

```bash
cd src/threads/build
make grade
```

## 开发规范

### 代码风格
- **语言**: C 和 x86 汇编。
- **格式**: 遵守项目的 `.clang-format`。在 `src/` 目录下使用 `make format` 格式化所有文件。
- **类型**: 使用 `<stdint.h>` 中的固定宽度类型（如 `uint32_t`、`int16_t`）。
- **内存**: 内核栈很小（是与 `struct thread` 共享的 4KB 页面的一部分）。避免大量的栈分配；使用 `malloc` 或 `palloc_get_page`。

### 同步
始终使用 `src/threads/synch.h` 中提供的原语：
- `struct semaphore` (信号量)
- `struct lock` (锁)
- `struct condition` (条件变量)

### 调试
- **Printf**: 使用 `printf` 进行控制台输出。
- **断言**: 使用 `ASSERT(condition)` 尽早捕获错误。
- **Panic**: 对于不可恢复的错误使用 `PANIC("message")`。
- **GDB**: 使用 `pintos --gdb` 运行，并在另一个终端使用 `pintos-gdb` 进行连接调试。

### 错误处理
系统调用应优雅地处理无效的用户指针和资源，并按照 Pintos 文档的规定返回 `-1` 或其他错误代码。
