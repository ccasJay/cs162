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
    .zero 4

bar:
    pushl %ebp
    movl %esp, %ebp
    subl $16, %esp
    movl 8(%ebp), %edx
    movl 12(%ebp), %eax
    addl %edx, %eax
    subl 16(%ebp), %eax
    movl %eax, -4(%ebp)
    movl -4(%ebp), %eax
    addl $1, %eax
    leave
    ret

foo:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    subl $4, %esp
    movl 8(%ebp), %edx
    movl 12(%ebp), %eax
    leal (%edx, %eax), %ebx
    subl $4, %esp
    pushl $5
    pushl $4
    pushl $3
    call bar
    addl $16, %esp
    addl %ebx, %eax
    movl %eax, p
    nop
    movl -4(%ebp), %ebx
    leave
    ret
```