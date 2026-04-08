/* Does absolutely nothing. */

#include "tests/lib.h"

int main(int argc UNUSED, char* argv[] UNUSED) { return 162; }

/* task5: Re-run GDB as before on do-nothing. Execute the loadusersymbols command, set a breakpoint at _start, and continue, to skip directly to the beginning of userspace execution. Using the disassemble and stepi commands, execute the do-nothing program instruction by instruction until you reach the int $0x30 instruction in proj-pregame/src/lib/user/syscall.c. At this point, print the top two words at the top of the stack by examining memory (Hint: x/2xw $esp) and copy the output.
(gdb) x/2xw $esp
0xbfffff98:     0x00000001      0x000000a2*/

/* task6: The int $0x30 instruction switches to kernel mode and pushes an interrupt stack frame onto the kernel stack for this process. Continue stepping through instruction-by-instruction until you reach syscall_handler. What are the values of args[0] and args[1], and how do they relate to your answer to the previous question?
(gdb) x/2xw $esp
0xc010bf64:     0xc010bfa4     0xc00221ea*/
//the data in the kernel stack
// 0xc010bfa4 是intr_entry在调用 intr_handler 时指向 struct intr_frame* 的指针
// 0xc00221ea 是call intr_handler 的返回地址
// args[0] args[1] equal to task5 output
// struct intr_frame是用户态切换到内核态前留下的快照.用户转道内核并不是转移数据，而是在内核stack 中存了一个指向用户态的pointer