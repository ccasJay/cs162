---
---
title: "Step through the crash"
source: "https://cs162.org/static/proj/proj-pregame/docs/tasks/step_through/"
author:
published:
created: 2026-04-05
description: "Pregame"
tags:
    - "clippings"
---

Now that we understand why the `do-nothing` program crashes, we will use GDB to step through the execution of the `do-nothing` test in Pintos, starting from when the kernel boots. Our goal is to find out how we can modify the Pintos user program loader so that `do-nothing` does not crash, while becoming acquainted with how Pintos supports user programs. To do this, change your working directory to `proj-pregame/src/userprog/` and run

```bash
FORCE_SIMULATOR=--bochs PINTOS_DEBUG=1 pintos-test do-nothing
```

Side note: You can alias it as `pintos-debug` by adding it to bashrc.

```bash
echo "alias pintos-debug='FORCE_SIMULATOR=--bochs PINTOS_DEBUG=1 pintos-test'" >> ~/.bashrc
```

GDB should now be open. At a high level, the following must happen before Pintos can start the `do-nothing` process:

- The BIOS reads the Pintos bootloader (`proj-pregame/src/threads/loader.S`) from the first sector of the disk into memory at address `0x7c00`.
- The bootloader reads the kernel code from disk into memory at address `0x20000` and then jumps to the kernel entrypoint (`proj-pregame/src/threads/start.s`).
- The code at the kernel entrypoint switches to 32-bit protected mode 1 and then calls main (`proj-pregame/src/threads/init.c`).
- The main function boots Pintos by initalizing the scheduler, memory subsystem, interrupt vector, hardware devices, and file system.

You’re welcome to read the code to learn more about this setup, but you don’t need to understand how this works for the Pintos projects or for this class.

Set a breakpoint at `run_task` and continue in GDB to skip the setup. As you can see in the code for `run_task`, Pintos executes the `do-nothing` program (specified on the Pintos command line), by invoking

```c
process_wait(process_execute("do-nothing"));
```

from `run_task`. Both `process_wait` and `process_execute` are in `proj-pregame/src/userprog/process.c`.

Now, answer the following questions:

1. Step into the `process_execute` function. What is the name and address of the thread running this function? What other threads are present in Pintos at this time? Copy their `struct thread`s.  
    *(Hint: `dumplist &all_list thread allelem` may be useful.)*

    ```text
    TODO: 1. do-nothing\000\000\000\000\000 , tid = 3
    2. pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_BLOCKED, name = "main", '\000' <repeats 11 times>, stack = 0xc000ee7c "", priority = 31,
        allelem = {prev = 0xc003b19c <all_list>, next = 0xc0104020}, elem = {prev = 0xc003cbb8 <temporary+4>, next = 0xc003cbc0 <temporary+12>}, pcb = 0xc010500c,
        magic = 3446325067}
    pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f14 "", priority = 0,
        allelem = {prev = 0xc000e020, next = 0xc010b020}, elem = {prev = 0xc003b18c <fifo_ready_list>, next = 0xc003b194 <fifo_ready_list+8>}, pcb = 0x0,
        magic = 3446325067}
    pintos-debug: dumplist #2: 0xc010b000 {tid = 3, status = THREAD_RUNNING, name = "do-nothing\000\000\000\000\000",
        stack = 0xc010bc94 "\330\274\020\300\264\274\020\300\374\024\002\300\220\274\020\300\344\274\020\300\t\022\002\300", priority = 31, allelem = {
          prev = 0xc0104020, next = 0xc003b1a4 <all_list+8>}, elem = {prev = 0xc003b18c <fifo_ready_list>, next = 0xc003b194 <fifo_ready_list+8>}, pcb = 0xc010502c,
        magic = 3446325067}
    ```

2. What is the backtrace for the current thread? Copy the backtrace from GDB as your answer.  
     TODO: 
     ```
     (gdb) bt
    #0  0xc00224d2 in intr0e_stub ()
    #1  0x00000005 in ?? ()
    ```

3. Set a breakpoint at `start_process` and continue to that point. What is the name and address of the thread running this function? What other threads are present in Pintos at this time? Copy their `struct thread` s.  
    ```text
    TODO: do-nothing\000\000\000\000\000 in 0xc010b000

    pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_BLOCKED, name = "main", '\000' <repeats 11 times>,
        stack = 0xc000ee7c "", priority = 31, allelem = {prev = 0xc003b19c <all_list>, next = 0xc0104020}, elem = {
            prev = 0xc003cbb8 <temporary+4>, next = 0xc003cbc0 <temporary+12>}, pcb = 0xc010500c, magic = 3446325067}
    pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>,
        stack = 0xc0104f14 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc010b020}, elem = {
            prev = 0xc003b18c <fifo_ready_list>, next = 0xc003b194 <fifo_ready_list+8>}, pcb = 0x0, magic = 3446325067}
    pintos-debug: dumplist #2: 0xc010b000 {tid = 3, status = THREAD_RUNNING, name = "do-nothing\000\000\000\000\000",
        stack = 0xc010bc94 "\330\274\020\300\264\274\020\300\374\024\002\300\220\274\020\300\344\274\020\300\t\022\002\300",
        priority = 31, allelem = {prev = 0xc0104020, next = 0xc003b1a4 <all_list+8>}, elem = {
            prev = 0xc003b18c <fifo_ready_list>, next = 0xc003b194 <fifo_ready_list+8>}, pcb = 0xc010502c, magic = 3446325067}
    ```

4. Step through the `start_process` function until you have stepped over the call to load. Note that load sets the `eip` and `esp` fields in the `if_` structure. Print out the value of the `if_` structure, displaying the values in hex (hint: `print/x if_`).  
     TODO: `$3 = {edi = 0x0, esi = 0x0, ebp = 0x0, esp_dummy = 0x0, ebx = 0x0, edx = 0x0, ecx = 0x0, eax = 0x0, gs = 0x23, fs = 0x23, es = 0x23, ds = 0x23, vec_no = 0x0,
  error_code = 0x0, frame_pointer = 0x0, eip = 0x804890f, cs = 0x1b, eflags = 0x202, esp = 0xc0000000, ss = 0x23}`

5. The first instruction in the `asm volatile` statement sets the stack pointer to the bottom of the `if_` structure. The second one jumps to `intr_exit`. The comments in the code explain what’s happening here. Step into the `asm volatile` statement, and then step through the instructions. As you step through the `iret` instruction, observe that the function “returns” into userspace. Why does the processor switch modes when executing this function? Feel free to explain this in terms of the values in memory and/or registers at the time `iret` is executed, and the functionality of the `iret` instruction.  
     TODO: 
     ```
     1. the return value is 0x8 ,after the iret, the value turns into 0x1b
     2. iret switches modes because the cs value (0x1b) it pops from the stack specifies user-mode privilege level, causing a privilege level change from kernel to user.
     ```

6. Once you’ve executed `iret`, type `info registers` to print out the contents of registers. Include the output of this command on Gradescope. How do these values compare to those when you printed out `if_`?  
     TODO: they are the same .the purpose of `iret` is the struct of `if_` pop to the cpu rigester
     ```
        eax            0x0                 0
    ecx            0x0                 0
    edx            0x0                 0
    ebx            0x0                 0
    esp            0xc0000000          0xc0000000
    ebp            0x0                 0x0
    esi            0x0                 0
    edi            0x0                 0
    eip            0x804890f           0x804890f
    eflags         0x202               [ IF ]
    cs             0x1b                27
    ss             0x23                35
    ds             0x23                35
    es             0x23                35
    fs             0x23                35
    gs             0x23                35
     ```

7. Notice that if you try to get your current location with `backtrace` you’ll only get a hex address. This is because because the debugger only loads in the symbols from the kernel. Now that we are in userspace, we have to load in the symbols from the Pintos executable we are running, namely `do-nothing`. To do this, use `loadusersymbols tests/userprog/do-nothing`. Now, using `backtrace`, you’ll see that you’re currently in the `_start` function. Using the `disassemble` and `stepi` commands, step through userspace instruction by instruction until the page fault occurs. At this point, the processor has immediately entered kernel mode to handle the page fault, so `backtrace` will show the current stack in kernel mode, not the user stack at the time of the page fault. However, you can use `btpagefault` to find the user stack at the time of the page fault. Copy down the output of `btpagefault`.  
     TODO:
     ```
     (gdb) btpagefault

    #0  0x08048915 in ?? ()

    #1  0xf000ff53 in ?? ()
    ```