## AT&T Cheat Sheet
- **Register**: prefix is `%`
- **Immediate**: prefix is `$`
- **Memory Address**: no prefix, use the syntax `offset(base, index, scale)`.  
  `scale` is the size of the data type, e.g., `4` for `int`, `8` for `long`/`double`.
- **`lea` instruction**: load effective address, operates on the memory address itself, not its contents.

**Instruction suffixes**:  
- `b` = byte (8 bits)  
- `w` = word (16 bits)  
- `l` = long (32 bits)

```asm
mov 8(%ebx), %eax              # Move value at [ebx + 8] to eax
mov %ecx, -4(%esi, %ebx, 8)    # Move ecx to [esi + ebx*8 - 4]

lea 8(%ebx), %eax              # Move address (ebx + 8) to eax, not the value at that address
```

#### General Purpose Registers (GPR)
Prefix: `e` = 32-bit, `r` = 64-bit, `x` = lower 16 bits.

| Name | Purpose             |
|------|---------------------|
| ax   | Accumulator         |
| bx   | Base                |
| cx   | Counter             |
| dx   | Data                |
| sp   | Stack Pointer       |
| bp   | Base Pointer        |
| si   | Source Index        |
| di   | Destination Index   |

---

#### Calling Convention (i386 System V ABI)

**Caller**  
Before `call`:
1. Save caller-saved GPRs to stack if needed (`pushl src`)
2. Push parameters to stack in reverse order (right-to-left), pad for 16-byte alignment

After `call`:
1. Remove parameters from stack
2. Restore caller-saved GPRs (`popl dest`)

**Callee**  
Before function logic:
1. Save old `%ebp` (`pushl %ebp`)
2. Allocate stack space for locals
3. Save callee-saved GPRs

After function logic:
1. Store return value in `%eax`
2. Restore callee-saved GPRs
3. Deallocate local variables
4. Restore caller's `%ebp` (`leave`)
5. Return to caller (`ret`)

---

#### Stack Frame Example

```cpp
int p = 0;

int bar(int x, int y, int z) {
    int w = x + y - z;
    return w + 1;
}

void foo(int a, int b) {
    p = a + b + bar(3, 4, 5);
}
```

```asm
p:
    .zero 4 // reserve 4 bytes for int p

bar:
    pushl %ebp   // Save old base pointer
    movl %esp, %ebp   // Set new base pointer
    subl $16, %esp  // Allocate space for local variable w and saved registers,use subl because stack grows downwards
    movl 8(%ebp), %edx  
    movl 12(%ebp), %eax   
    addl %edx, %eax
    subl 16(%ebp), %eax  //this step is to subtract z from the sum of x and y, since z is located at 16(%ebp)  
    movl %eax, -4(%ebp)   // Store w in local variable
    movl -4(%ebp), %eax   // Move w to return value register
    addl $1, %eax
    leave
    ret

foo:
    pushl %ebp 
    movl %esp, %ebp
    pushl %ebx  // Save callee-saved register ebx
    subl $4, %esp   
    movl 8(%ebp), %edx  // Move a to edx
    movl 12(%ebp), %eax   //Move b to eax
    leal (%edx, %eax), %ebx   // Compute a + b and store in ebx
    subl $4, %esp  // 压入参数前，先point下移4字节确保stack对齐
    pushl $5
    pushl $4
    pushl $3
    call bar
    addl $16, %esp  // Clean up parameters from stack (3 parameters * 4 bytes each)
    addl %ebx, %eax // Add a + b to the return value of bar
    movl %eax, p // Store result in global variable p
    nop   // No operation, can be used for padding
    movl -4(%ebp), %ebx
    leave
    ret
```