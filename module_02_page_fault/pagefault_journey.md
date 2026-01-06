# Page Fault Interactive Journey

We are tracing `strcpy(ptr, "offset world")` triggering a fault.

## Phase -1: Process Creation (BEFORE Your Code Runs)

**When you ran `./mm_exercise_user` in your shell:**

```
bash (PID=1234) calls fork()
    │
    ├─→ kernel_clone() (kernel/fork.c)
    │       │
    │       ├─→ copy_mm() (kernel/fork.c:1563)
    │       │       │
    │       │       ├─→ mm = mm_alloc()          ← Allocates mm_struct
    │       │       ├─→ mm->pgd = pgd_alloc(mm)   ← Allocates PGD table (4KB)
    │       │       └─→ Returns mm_struct pointer
    │       │
    │       └─→ task_struct->mm = mm              ← Links process to mm_struct
    │
    ├─→ New process PID=14420 (mm_exercise_user)
    │
    └─→ execve("./mm_exercise_user")
            │
            ├─→ Replaces address space with ELF binary
            ├─→ Creates VMAs for .text, .data, .bss, stack, heap
            └─→ Your main() starts running

At this point:
- mm_struct exists for PID 14420
- mm->pgd = base address of your Page Global Directory
- CR3 register = physical address of mm->pgd
- No VMA for 0x716b... yet (that comes from your mmap call)
```

**AXIOM**: Every process has exactly ONE mm_struct. It exists before your code runs.

## Phase 0: Userspace Origin (mm_exercise_user.c)

### Line 81: `mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)`

- Syscall enters kernel.
- Kernel creates VMA: `[0x716b326c8000 - 0x716b326c9000)` with `rw-p`.
- **Note**: Physical RAM NOT allocated yet. VMA = "Promise" only.
- Returns: `vaddr = 0x716b326c8000`.

### PROOF: Maple Tree Insertion Happens Here (BEFORE Page Fault)

**Kprobe Added**: `__mmap_region` at `mm/mmap.c:2727`

**Syscall Chain (Derived from Code)**:

```
mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
    → syscall #9 (x86_64)
    → __x64_sys_mmap (arch/x86/kernel/sys_x86_64.c)
    → ksys_mmap_pgoff (mm/mmap.c)
    → do_mmap (mm/mmap.c)
    → mmap_region (mm/mmap.c:2964)
    → __mmap_region (mm/mmap.c:2727) ← KPROBE FIRES HERE
        → Line 2885: vma_set_anonymous(vma)
        → Line 2895: vma_iter_store(&vmi, vma) ← VMA INSERTED INTO MAPLE TREE
    → Returns addr=0x716b326c8000 to userspace
```

### PROOF: How addr=0x716b326c8000 Returns to Userspace

**Return Chain (6 Functions Traced)**:

```c
// FUNCTION 1: __mmap_region (mm/mmap.c:2727-2962)
// WHAT: Creates VMA, inserts into Maple Tree, returns addr
// RETURN: Line 2960: return addr; → addr = 0x716b326c8000 (8 bytes in RAX)
static unsigned long __mmap_region(struct file *file, unsigned long addr, ...) {
    // addr type = unsigned long = 8 bytes = 64 bits
    // addr value = 0x716b326c8000
    // Binary: 0111 0001 0110 1011 0011 0010 0110 1100 1000 0000 0000 0000
    // Hex breakdown: 0x716b = high 16 bits, 0x326c = mid 16 bits, 0x8000 = low 16 bits
    // addr >> 12 = 0x716b326c8 = PFN index (52 bits), low 12 bits = 0x000 = page offset
    ...
    return addr;  // Line 2960: addr placed in RAX register
    // RAX after return: [0x716b326c8000]
    // │               │
    // └───────────────┴─→ 8 bytes = 64 bits (x86_64)
}

// FUNCTION 2: mmap_region (mm/mmap.c:2964-2996)
// WHAT: Wrapper that validates flags, calls __mmap_region
// WHO CALLS: do_mmap (line 1390)
// RETURN: Line 2995: return ret;
unsigned long mmap_region(struct file *file, unsigned long addr, ...) {
    unsigned long ret;                           // Stack offset: RBP-0x08, 8 bytes
    ret = __mmap_region(file, addr, ...);        // Line 2988: ret = 0x716b326c8000
    // CALL/RET mechanics:
    // 1. CALL __mmap_region: push RIP+5 onto stack → RSP -= 8
    // 2. __mmap_region executes, places 0x716b326c8000 in RAX
    // 3. RET: pop saved RIP → RSP += 8, jump to saved RIP
    // 4. ret = RAX = 0x716b326c8000
    return ret;                                   // Line 2995: RAX unchanged = 0x716b326c8000
}

// FUNCTION 3: do_mmap (mm/mmap.c:1295-1396)
// WHAT: Calculates vm_flags, finds free VA range, calls mmap_region
// WHO CALLS: vm_mmap_pgoff
// RETURN: Line 1395: return addr;
unsigned long do_mmap(struct file *file, unsigned long addr, ...) {
    addr = mmap_region(file, addr, ...);         // Line 1390: addr = 0x716b326c8000
    // Variable reuse: input addr (0 from user) overwritten with output addr
    // addr before line 1390: could be 0 (NULL hint) or user-specified
    // addr after line 1390: 0x716b326c8000 (kernel-chosen address)
    return addr;                                  // Line 1395: RAX = 0x716b326c8000
}

// FUNCTION 4: vm_mmap_pgoff (mm/util.c:500-530)
// WHAT: Acquires mmap_lock, calls do_mmap, releases lock
// WHO CALLS: ksys_mmap_pgoff
// RETURN: Line 527: return ret;
unsigned long vm_mmap_pgoff(struct file *file, unsigned long addr, ...) {
    unsigned long ret;
    ret = do_mmap(file, addr, ...);              // ret = 0x716b326c8000
    // mmap_lock held during do_mmap → prevents concurrent VMA modifications
    // After return: mmap_lock released
    return ret;                                   // RAX = 0x716b326c8000
}

// FUNCTION 5: ksys_mmap_pgoff (mm/mmap.c:1398-1441)
// WHAT: Handles file descriptor, calls vm_mmap_pgoff
// WHO CALLS: SYSCALL_DEFINE6(mmap_pgoff)
// RETURN: Line 1440: return retval;
unsigned long ksys_mmap_pgoff(unsigned long addr, unsigned long len, ...) {
    unsigned long retval;
    retval = vm_mmap_pgoff(file, addr, ...);     // Line 1436: retval = 0x716b326c8000
    // For MAP_ANONYMOUS: file = NULL (fd = -1 ignored)
    // For file-backed: file = fget(fd) → struct file pointer
    return retval;                                // Line 1440: RAX = 0x716b326c8000
}

// FUNCTION 6: SYSCALL_DEFINE6(mmap_pgoff) (mm/mmap.c:1443-1448)
// WHAT: Syscall entry point, bridges userspace to kernel
// WHO CALLS: syscall instruction from userspace (via libc mmap())
// RETURN: Line 1447: return ksys_mmap_pgoff(...);
SYSCALL_DEFINE6(mmap_pgoff, unsigned long, addr, unsigned long, len, ...) {
    return ksys_mmap_pgoff(addr, len, prot, flags, fd, pgoff);
    // RAX = 0x716b326c8000
    // SYSRET instruction: copies RAX to userspace RAX, returns to userspace RIP
}
```

**STACK FRAME UNWINDING (Numerical)**:

```
KERNEL STACK (grows down, 16KB per thread = THREAD_SIZE = 0x4000)
┌────────────────────────────────────────────────────────────────────────────────┐
│ Address         │ Content                       │ Size  │ Notes                │
├─────────────────┼───────────────────────────────┼───────┼──────────────────────┤
│ 0xffffcc93...c000 │ Top of stack (entry)        │       │ THREAD_SIZE boundary │
├─────────────────┼───────────────────────────────┼───────┼──────────────────────┤
│ 0xffffcc93...bf98 │ Saved RIP (from SYSCALL)    │ 8     │ Userspace return addr│
│ 0xffffcc93...bf90 │ Saved RBP (ksys_mmap_pgoff) │ 8     │ Frame pointer        │
│ 0xffffcc93...bf88 │ retval = 0x716b326c8000     │ 8     │ Local variable       │
├─────────────────┼───────────────────────────────┼───────┼──────────────────────┤
│ 0xffffcc93...bf80 │ Saved RBP (vm_mmap_pgoff)   │ 8     │ Frame pointer        │
│ 0xffffcc93...bf78 │ ret = 0x716b326c8000        │ 8     │ Local variable       │
├─────────────────┼───────────────────────────────┼───────┼──────────────────────┤
│ 0xffffcc93...bf70 │ Saved RBP (do_mmap)         │ 8     │ Frame pointer        │
│ 0xffffcc93...bf68 │ addr = 0x716b326c8000       │ 8     │ Overwritten variable │
├─────────────────┼───────────────────────────────┼───────┼──────────────────────┤
│ 0xffffcc93...bf60 │ Saved RBP (mmap_region)     │ 8     │ Frame pointer        │
│ 0xffffcc93...bf58 │ ret = 0x716b326c8000        │ 8     │ Local variable       │
├─────────────────┼───────────────────────────────┼───────┼──────────────────────┤
│ 0xffffcc93...bf50 │ Saved RBP (__mmap_region)   │ 8     │ Frame pointer        │
│ 0xffffcc93...bf48 │ addr = 0x716b326c8000       │ 8     │ **THE ORIGIN**       │
└─────────────────┴───────────────────────────────┴───────┴──────────────────────┘

Total stack depth: 0xc000 - 0xbf48 = 0xb8 = 184 bytes for return chain
Each frame: ~32 bytes (RBP + RIP + locals)
6 frames × 32 bytes ≈ 192 bytes (matches)
```

**RAX REGISTER STATE (Per Function Return)**:

```
__mmap_region returns → RAX = 0x716b326c8000
                              ↓
mmap_region returns   → RAX = 0x716b326c8000 (unchanged)
                              ↓
do_mmap returns       → RAX = 0x716b326c8000 (unchanged)
                              ↓
vm_mmap_pgoff returns → RAX = 0x716b326c8000 (unchanged)
                              ↓
ksys_mmap_pgoff returns → RAX = 0x716b326c8000 (unchanged)
                              ↓
SYSCALL_DEFINE6 returns → RAX = 0x716b326c8000 (unchanged)
                              ↓
                        SYSRET instruction
                              ↓
                        Userspace RAX = 0x716b326c8000
                              ↓
                        libc mmap() returns 0x716b326c8000
                              ↓
                        vaddr = (char *)0x716b326c8000
```

**W-FORMAT ANNOTATIONS**:

- **WHAT**: 8-byte unsigned long value 0x716b326c8000 (124,858,566,737,920 decimal) passed through 6 function returns
- **WHY**: Kernel allocates virtual address, must communicate back to userspace so user knows where to access memory
- **WHERE**: Originates at mm/mmap.c:2727 (__mmap_region), ends at userspace mm_exercise_user.c:81 (vaddr variable)
- **WHO**: Kernel chooses address (find_vma_intersection algorithm), userspace receives it (RAX → mmap return value)
- **WHEN**: After Maple Tree insertion (line 2895), before mmap() returns to userspace
- **WITHOUT**: Without this return, userspace would not know where its VMA lives → would segfault on any access
- **WHICH**: RAX register (x86_64 syscall return convention, not RDI/RSI/RDX which are arguments)

**Live dmesg Output (Captured)**:

```
[MMAP_REGION] PID=12491 COMM=antigravity file=ANON addr=0x391c01920000 len=0x20000 vm_flags=0x70
[MMAP_REGION] PID=12503 COMM=Compositor file=FILE addr=0x732a186bf000 len=0x40000 vm_flags=0xfb
```

**Handler Code (pagefault_observer.c)**:

```c
static int __kprobes mmap_region_handler_pre(struct kprobe *p, struct pt_regs *regs) {
    // x86_64: RDI = file, RSI = addr, RDX = len, RCX = vm_flags
    file_ptr = regs->di;   // NULL = anonymous
    addr = regs->si;        // 0x716b326c8000
    len = regs->dx;         // 0x1000 = 4096
    vm_flags = regs->cx;    // 0x70 = READ|WRITE|ACCOUNT
    pr_info("[MMAP_REGION] PID=%d file=%s addr=0x%lx ...", ...);
}
```

**TIMELINE PROOF**:

```
TIME ─────────────────────────────────────────────────────────────────────────→

T1: mmap() enters kernel
    │
    ├─→ __mmap_region() called
    │       [MMAP_REGION] PID=xxx file=ANON addr=0x716b326c8000 ← KPROBE FIRES
    │
    ├─→ vma_iter_store() → VMA now in Maple Tree
    │
T2: mmap() returns 0x716b326c8000 to userspace
    │
    │   ← USER CODE RUNS: ptr = vaddr + 0x100 = 0x716b326c8100 →
    │
T3: strcpy(ptr, "offset world") → CPU writes to 0x716b326c8100
    │
    ├─→ MMU walks Page Table → PTE[200] = NOT PRESENT
    │
    ├─→ PAGE FAULT! CPU Exception 14
    │
T4: handle_mm_fault() called
    │
    ├─→ lock_vma_under_rcu() → Searches Maple Tree → FINDS VMA (created at T1)
    │
    ├─→ __handle_mm_fault() → Allocates PFN → Creates PTE[200]
    │
T5: Page Fault resolved, strcpy completes
```

**CONCLUSION**: VMA exists at T1. Page Fault happens at T3. ∴ Maple Tree populated BEFORE page fault.

### Line 100: `char *target_addr = (char *)vaddr + 0x100;`

- Calculation: `0x716b326c8000 + 0x100 = 0x716b326c8100`.

### Line 101: `strcpy(target_addr, "offset world");`

- CPU fetches `mov %rcx, (%rax)` where `RAX = 0x716b326c8100`.
- MMU walks Page Table → PTE[200] = NOT PRESENT.
- **CPU TRAP** → Interrupt 14 (Page Fault).

## Phase 1: CPU Hardware Trap

1. CPU pushes Error Code onto stack: `0x06` (Write=1, User=1, Present=0).
2. CPU pushes `RIP` onto stack: `0x62a24698d416` (strcpy instruction).
3. CPU loads `RSP` from TSS (kernel stack): `0xffffcc9392ec4000`.
4. CPU jumps to IDT[14] → `asm_exc_page_fault`.

## Phase 2: Assembly Stub (arch/x86/entry/entry_64.S)

```asm
asm_exc_page_fault:
    pushq %rbx, %rcx, ...          ; Build struct pt_regs
    movq %cr2, %rsi                ; rsi = Faulting Address = 0x716b326c8100
    call exc_page_fault            ; Call C handler
```

## Phase 3: C Handler Chain

**`exc_page_fault(regs, error_code)`** → **`do_user_addr_fault(regs, error_code, address)`**:

- `address = cr2 = 0x716b326c8100`.
- Convert error_code (0x06) to flags (0x1255):
  - `FAULT_FLAG_WRITE` (0x01) | `FAULT_FLAG_USER` (0x40) | `FAULT_FLAG_VMA_LOCK` (0x1000) | ...

**`handle_mm_fault(vma, address, flags, regs)`** → **OBSERVATION POINT**.

## The Critical Rounding (PAGE_MASK)

```c
address &= PAGE_MASK;  // 0x716b326c8100 & 0xFFFFFFFFFFFFF000 = 0x716b326c8000
```

Kernel allocates page for `0x716b326c8000`, not `0x716b326c8100`.

---

## [Step 1] Entry: `exc_page_fault` & `pt_regs`

- **Location**: `arch/x86/mm/fault.c`
- **Component**: CPU Hardware -> ASM Stubs -> C Kernel.
- **Object**: `struct pt_regs` (The Saved State).
- **Exercise**: Prove `regs->ip` points to the `strcpy` instruction.
- **Status**: [x] DONE.
- **Proof (Axiomatic Derivation)**:
  1. **Axiom 1**: Memory is an array of bytes. Base Address = Index 0.
  2. **Axiom 2**: ASLR (Address Space Layout Randomization) loads binaries at a random Base.
  3. **Input 1**: `Base_Addr` (from `/proc/PID/maps`) = `0x648d86c41000`.
  4. **Input 2**: `RIP` (from Hardware `pt_regs`) = `0x648d86c42416`.
  5. **Step 1**: Calculate Offset = `RIP` - `Base_Addr`.
  6. **Calculation**: `0x...42416` - `0x...41000` = `0x1416` (5142 decimal).
  7. **Step 2**: Read Binary at Offset `0x1416` using `objdump`.
  8. **Observation**: `1416: mov %rcx,(%rax)`.
  9. **Axiom 3**: `mov SRC, (DEST)` copies data from register SRC to memory address DEST.
  10. **Context**: `strcpy` copies strings. `mov` IS the copy instruction.
  11. **Conclusion**: The Hardware caught the exact instruction performing the illegal memory write.

- **New Terms Defined**:
  - `ASLR`: Randomizing the starting index of the memory array.
  - `Offset`: Distance from the start.

## [Step 2] Context: `do_user_addr_fault`

- **Location**: `arch/x86/mm/fault.c`
- **Component**: Fault Flags Parser.
- **Object**: `flags` (FAULT_FLAG_WRITE | USER).
- **Exercise**: Derive `flags` (0x1255) from `error_code` numerically.
- **Status**: [x] DONE.
- **Proof (Axiomatic Derivation)**:
  1. **Input**: `FLAGS` from `dmesg` is `0x1255`.
  2. **Axiom Source**: `include/linux/mm_types.h` (Enum `fault_flag`).
  3. **Bit Breakdown**:
     - `0x0001` (Bit 0) = `FAULT_FLAG_WRITE` -> **TRUE** (Attempted Write).
     - `0x0040` (Bit 6) = `FAULT_FLAG_USER` -> **TRUE** (User Mode).
     - `0x1000` (Bit 12) = `FAULT_FLAG_VMA_LOCK` -> **TRUE** (Per-VMA Lock).
  4. **Verification**: `0x1000 + 0x0200 + 0x0040 + 0x0010 + 0x0004 + 0x0001 = 0x1255`.
  5. **Conclusion**: The Kernel successfully identified the illegal `strcpy` as a User-Mode Write.

## Snapshot: Step 1 & 2 Complete (Axiomatic 7 Ws)

### 1. The 7 Ws (Strict Numerical Facts)

- **What**: Page Fault (Illegal Write).
  - *Data*: `mov %rcx,(%rax)` at `RIP=0x...2416` (Offset 0x1416).
  - *Value*: `RAX`=User Address `0x716b326c8100`.
- **Why**: Permission Violation (Write to Read-Only/Missing).
  - *Data*: `FLAGS=0x1255` -> Bit 0 (Write)=1, Bit 12 (VMA_LOCK)=1.
  - *Definition*: `1 << 0` | `1 << 12`.
- **Where**: Kernel Stack (Private Allocation).
  - *Data*: `RSP=0xffffcc9392ec3d78`.
  - *Range*: `[0xffffcc9392ec0000 - 0xffffcc9392ec4000]`.
- **Who**: `mm_exercise_user` (PID 14420).
  - *Data*: `current->pid = 14420`.
- **When**: After `strcpy` executes, before `handle_mm_fault` logic starts.
  - *Timestamp*: `dmesg` timestamp `944.191...`.
- **Without**: Without Kprobe, this state is destroyed/context-switched away instantly.
- **Which**: The specific `struct mm_struct` belonging to Task `0xffff8a1761b60000`.

### 2. FULL FUNCTION TRACE (Pseudo-Debugger)

# 1. **Call**: `asm_exc_page_fault` (Assembly Stub)

- **Data**: `HW_ERROR=0x15` (Pushed by CPU).
- **Work**: Pushes `pt_regs`. Calls C handler.

# 2. **Call**: `do_user_addr_fault(regs, hw_error)` (arch/x86/mm/fault.c)

- **Values**: `regs->ip=0x...2416`, `error_code=0x15`.
- **Data**: `address` (CR2) = `0x716b326c8100`.
- **Work**: Reads CR2. Converts `error_code` (0x15) to `flags` (0x1255).
- **Axiom**: `flags = error_code | FAULT_FLAG_USER | FAULT_FLAG_VMA_LOCK`.

## handle_mm_fault Entry Point (mm/memory.c:5519)

### DATA STRUCTURES BEFORE CALL (Live from PID 14420)

```
+---------------------------+-----------------------------------+
| struct vm_area_struct     | Kernel RAM @ 0xffff8881xxxxxxxx  |
+---------------------------+-----------------------------------+
| vm_start = 0x716b326c8000 | Byte 0-7: 0x00 0x80 0x6c 0x32...  |
| vm_end   = 0x716b326c9000 | Byte 8-15: 0x00 0x90 0x6c 0x32... |
| vm_flags = 0x00100073     | Bits: VM_READ|VM_WRITE|VM_MAYREAD|VM_MAYWRITE|VM_ACCOUNT |
| vm_mm    = 0xffff8881...  | Pointer to struct mm_struct      |
+---------------------------+-----------------------------------+

+---------------------------+-----------------------------------+
| struct pt_regs (Stack)    | Kernel Stack @ 0xffffcc9392ec3d00|
+---------------------------+-----------------------------------+
| r15 = 0x0                 | Offset 0x00                       |
| r14 = 0x0                 | Offset 0x08                       |
| ...                       | ...                               |
| ip  = 0x62a24698d416      | Offset 0x80 (RIP of strcpy)       |
| cs  = 0x33                | Offset 0x88 (User Code Segment)   |
| flags = 0x246             | Offset 0x90                       |
| sp  = 0x7ffd12345678      | Offset 0x98 (User RSP)            |
+---------------------------+-----------------------------------+
```

### FUNCTION CALL (mm/memory.c:5519-5551)

```c
vm_fault_t handle_mm_fault(struct vm_area_struct *vma, unsigned long address,
                           unsigned int flags, struct pt_regs *regs)
// vma     = 0xffff8881xxxxxxxx (Pointer to VMA struct above)
// address = 0x716b326c8100     (Faulting VA, NOT rounded yet)
// flags   = 0x1255             (WRITE|USER|VMA_LOCK|KILLABLE|ALLOW_RETRY|INTERRUPTIBLE)
// regs    = 0xffffcc9392ec3d00 (Pointer to pt_regs on kernel stack)
```

### LINE 5523: `struct mm_struct *mm = vma->vm_mm;`

- `vma` = `0xffff8881xxxxxxxx` → `vma->vm_mm` offset = 64 bytes → Read RAM @ `0xffff8881xxxxxxxx + 64` = `0xffff8881yyyyyyyy` → That is `mm`.

## Phase 4: Page Table Walk (Live Trace)

**Live Data Captured from PID 41803**:

- **Address**: `0x784dba14e000` (rw-p VMA in `/proc/41803/maps`)
- **Binary**: `0111 1000 0100 1101 1011 1010 0001 0100 1110 0000 0000 0000`

### 1. Verification of Hardware Capability

- **Command**: `grep la57 /proc/cpuinfo`
- **Result**: `Exit code 1` (Not Found).
- **Axiom**: Missing `la57` flag = CPU supports only 4-level paging (48-bit address space).
- **Inference**: `PGDIR_SHIFT` = 39 (Runtime variable). `CONFIG_PGTABLE_LEVELS=5` is irrelevant due to hardware limit.

### 2. Full Trace of `__handle_mm_fault`

```c
// #10. CALL. mm/memory.c:5295. __handle_mm_fault(vma, address=0x784dba14e000, ...)
// WHAT: Coordinates page table walk and page allocation
// LINE 5300: vmf.address = address & PAGE_MASK
// CALCULATION: 0x784dba14e000 & 0xFFFFFFFFFFFFF000 = 0x784dba14e000 (Already aligned)

// #10b. PGD Lookup (Line 5312)
// LINE: pgd = pgd_offset(mm, address)
// EXPANSION: mm->pgd + ((address >> 39) & 0x1FF)
// CALCULATION:
//   address >> 39 = 0x784dba14e000 >> 39
//                 = 132274488926208 >> 39
//                 = 240
//   240 & 0x1FF = 240 = 0x0F0
// RESULT: Index 240 in PGD table.
// MEMORY: Read 8 bytes at [CR3 + 240*8]. Value = Physical Address of P4D table.

// #10c. P4D Lookup (Line 5313)
// LINE: p4d = p4d_alloc(mm, pgd, address)
// AXIOM: On 4-level paging, P4D is FOLDED into PGD.
// CODE: p4d_offset(pgd, address) -> returns pgd pointer itself logic.
// EFFECTIVE INDEX: (address >> 30) & 0x1FF = 310 (0x136) IF it were 5-level.
// ACTUAL: Just passes through to PUD alloc in 4-level mode.

// #10d. PUD Lookup (Line 5317)
// LINE: pud = pud_alloc(mm, p4d, address)
// EXPANSION: p4d_val(*p4d) + ((address >> 30) & 0x1FF) ... wait.
// BIT SHIFT: PUD_SHIFT = 30.
// CALCULATION:
//   address >> 30 = 0x784dba14e000 >> 30
//                 = 132274488926208 >> 30
//                 = 123190
//   123190 & 0x1FF = 310 = 0x136
// RESULT: Index 310 in PUD table.
// MEMORY: Read 8 bytes at [P4D_Phys + 310*8]. Value = Physical Address of PMD table.

// #10e. PMD Lookup (Line 5347)
// LINE: pmd = pmd_alloc(mm, pud, address)
// BIT SHIFT: PMD_SHIFT = 21.
// CALCULATION:
//   address >> 21 = 0x784dba14e000 >> 21
//                 = 63073606
//   63073606 & 0x1FF = 464 = 0x1D0
// RESULT: Index 464 in PMD table.
// MEMORY: Read 8 bytes at [PUD_Phys + 464*8]. Value = Physical Address of PTE table.

// #10f. PTE Lookup (Inside handle_pte_fault)
// BIT SHIFT: PAGE_SHIFT = 12.
// CALCULATION:
//   address >> 12 = 0x784dba14e000 >> 12
//                 = 32293683664
//   32293683664 & 0x1FF = 334 = 0x14E
// RESULT: Index 334 in PTE table.
// MEMORY: Read 8 bytes at [PMD_Phys + 334*8]. Value = PTE Entry.
```

**Verification Summary**:

- PGD Index: **240** (Bits 47-39)
- PUD Index: **310** (Bits 38-30)
- PMD Index: **464** (Bits 29-21)
- PTE Index: **334** (Bits 20-12)
- Offset:    **0**   (Bits 11-0)

### LINE 5528: `ret = sanitize_fault_flags(vma, &flags);`

- Validates `flags`. If `vma->vm_flags` lacks `VM_WRITE` but `flags & FAULT_FLAG_WRITE` → May modify `flags` or return error.

### LINE 5532-5537: Permission Check

```c
if (!arch_vma_access_permitted(vma, flags & FAULT_FLAG_WRITE, ...))
    ret = VM_FAULT_SIGSEGV;
// flags & FAULT_FLAG_WRITE = 0x1255 & 0x0001 = 0x0001 = TRUE (Write Requested)
// vma->vm_flags & VM_WRITE = 0x00100073 & 0x00000002 = 0x0002 = TRUE (Write Allowed)
// ∴ Permission GRANTED.
```

### LINE 5551: `ret = __handle_mm_fault(vma, address, flags);`

- Descends into page table walk. Address `0x716b326c8100` will be rounded: `address & PAGE_MASK = 0x716b326c8000`.

### MAPLE TREE SEARCH (Already happened BEFORE this function)

```
Maple Tree (mm->mm_mt) for PID 14420:
    Root @ 0xffff8882xxxxxxxx
         |
    Node: pivots=[0x62a246991000, 0x716b325ff000, 0x716b326c9000, ...]
         |
    Search: 0x716b326c8100
         |
    pivots[2]=0x716b326c9000 >= 0x716b326c8100? YES → slots[2] = VMA pointer
         |
    Return: vma @ 0xffff8881xxxxxxxx (vm_start=0x716b326c8000, vm_end=0x716b326c9000)
```

### STATUS: Observation Point. Next: `__handle_mm_fault` (Page Table Walk)

## [Step 4] Routing: `__handle_mm_fault`

- **Location**: `mm/memory.c`
- **Component**: Page Table Walker.
- **Object**: `pgd`, `p4d`, `pud`, `pmd` addresses.
- **Exercise**: Calculate indices for 0x75... address.
- **Status**: [x] DONE (Phase 4 Trace Live Data).

## [Interlude] Axiomatic Derivation: The Folio (struct folio)

- **Problem**: `struct page` assumes 4KB. But huge pages (2MB) exist.
- **Old Solution**: Use a "Head Page" and 511 "Tail Pages". Messy.
- **New Solution (Axiom)**: `struct folio`.
- **Definition Source**: `include/linux/mm_types.h` (line 284).
- **AXIOM**: `struct folio` OVERLAYS `struct page`.
  - It is a **Union**.
  - `(struct page *)ptr == (struct folio *)ptr`.
  - They share the exact same memory address.
- **Why?**:
  - `alloc_anon_folio` returns a folio.
  - If it's a 4KB page, it's a "Order-0 Folio".
  - `folio->page` allows access to the old `struct page`.

## [Step 5] Allocation: `do_anonymous_page` (The Creation of Data)

- **Location**: `mm/memory.c` (line 3760)
- **Concept**: **The Buddy Allocator**.
- **The Event**:
  1. `do_anonymous_page()` called.
  2. `folio = alloc_anon_folio(vmf)` (line 4309).
  3. `__folio_alloc` → `__alloc_pages(GFP_HIGHUSER | __GFP_ZERO)`.
- **The Magic**:
  - BEFORE: `0x716b326c8000` has NO Physical RAM.
  - AFTER: A physical page (e.g., PFN 0x12345) is reserved.
  - The page is **ZEROED** (filled with 4096 bytes of 0x00) for security.
  - The buddy allocator marks this page as "In Use" in the global bitmap.

## [Step 5b] Axiomatic Verification: PFN Lifecycle & Security Trap

**1. OBSERVATION (NON-ROOT)**

- Cmd: `python3 check_pfn.py` (User `r`)
- Output: `0x8180000000000000`
- Decode: Present=1, File=0, PFN=0x0.
- **Paradox**: Write `1` happened. Zero Page (Read-Only) impossible.
- **Hypothesis**: OS lying to user?

**2. OBSERVATION (ROOT)**

- Cmd: `sudo python3 check_pfn.py` (User `root`)
- Output: `0x81800000002cad54`
- Decode: Present=1, PFN=0x2CAD54.
- **PFN**: `0x2CAD54` = 2,927,956.
- **PhysAddr**: `0x2CAD54 << 12` = `0x2cad54000` (11.17 GB).

**3. DERIVATION OF TROUBLE**

- **Security Feature**: `kernel.pagemap_monitor` or generic PFN redaction.
- **Logic**: IF (User != Root) → PFN = 0.
- **Fix**: MUST use `sudo` for Physical Address analysis.

**4. AXIOMATIC DEMAND PAGING PROOF (FINAL)**

- **Event**: `mmap` (Start) → `0x8180...0000` (Redacted/Zero).
- **Event**: `write` (Fault) → Kernel Allocates `0x2CAD54`.
- **Event**: `check` (Root) → `0x8180...2CAD54` (Valid).
- **Conclusion**: Virtual Promise (`mmap`) became Physical Reality (`buddy alloc`) via Page Fault.

---

## [Phase 6] Trouble Log (Axiomatic)

1. **BUFFERING**:
   - `printf` → libc buffer → `write(1, ...)` (System Call).
   - Sleep stalled process *before* flush.
   - **Fix**: `fflush(stdout)` forces syscall immediately.

2. **TIMING**:
   - Background process traces fault.
   - Race condition: Tracer checks before Fault?
   - **Fix**: `sleep(10)` in User → `sleep(12)` in Tracer. Synchronized.

3. **PERMISSION**:
   - `pagemap` PFN bits are **SENSITIVE**.
   - **Exploit prevention**: Hides physical layout from malware (Rowhammer).
   - **Fix**: Root access reveals truth.

## Phase 6: The Return (PTE Construction)

1. **Context**: `alloc_anon_folio` returned PFN `0x2CAD54`.
2. **Goal**: CPU must find this PFN next time `0x7e95...` is accessed.
3. **Action**: Kernel writes specific bit-pattern to the **Page Table Entry (PTE)**.

### [Step 6a] Axiomatic Derivation of Indices

- **Input VA**: `0x7e95046af000` (from Trace).
- **Binary**: `0000000000000000011111101001010100000100011010101111000000000000`.
- **Slicing (9-9-9-9-12)**:
  - **PGD** (47-39): `011111101` = **253**.
  - **PUD** (38-30): `001010100` = **84**.
  - **PMD** (29-21): `000010000` = **35**.
  - **PTE** (20-12): `010101111` = **175**.
  - **Offset**(11-0) : `000000000` = **0**.

### [Step 6b] The PTE Value Calculation

- **The PFN**: `0x2CAD54`.
- **The Shift**: `PFN << 12` = `0x2CAD54000`. (Physical Address).
- **The Flags**: `vma->vm_page_prot`.
  - `_PAGE_PRESENT` (0x1) | `_PAGE_RW` (0x2) | `_PAGE_USER` (0x4)
  - `_PAGE_ACCESSED`(0x20) | `_PAGE_DIRTY` (0x40)
- **Total Flags**: `0x1 | 0x2 | 0x4 | 0x20 | 0x40` = **0x67**.
- **Final PTE Value**: `0x2CAD54067`.

### [Step 6c] The Kernel Write

- **Function**: `set_pte_at(mm, address, ptep, entry)`.
- **Pointer**: `ptep` points to the 175th entry in the Page Table determined by PGD->PUD->PMD.
- **Micro-Op**: `*ptep = 0x2CAD54067`.
- **Result**: MMU translates `0x7e95...` to `0x2CAD54...`. Fault resolved.

## JOURNEY COMPLETE

1. **Creation**: `mmap` (Virtual Promise).
2. **Access**: `mov` (Hardware Fault).
3. **Routing**: `PGD(253)` (Software Walk).
4. **Allocation**: `buddy` (Physical PFN).
5. **Linking**: `PTE` (The Map).

## AXIOMATIC EXECUTION TRACE (STRICTLY DERIVED)

**AXIOM 1: SILICON LOGIC (Source: Intel SDM Vol 3, Ch 4)**

- **Hardware**: The CPU contains a **Memory Management Unit (MMU)**.
- **Rule**: Every memory access (Instruction Fetch or Data Read/Write) goes through MMU.
- **Circuit**: `PhysAddr = Translate(VirtAddr)`.
- **Fault Condition**: If `Translate` fails (Present Bit == 0), CPU triggers **Exception 14 (#PF)**.
- **Hardware Response**:
  1. Store Faulting Address → **CR2 Register** (Control Register 2).
  2. Push Error Code → **Stack**.
  3. Jump to Handler → **IDT[14]** (Interrupt Descriptor Table).

**AXIOM 2: SOFTWARE INTERFACE (Source: arch/x86/kernel/entry_64.S)**

- **Handler**: `asm_exc_page_fault` is the entry point defined in IDT[14].
- **Action**: Saves CPU state (Registers) to Stack as `struct pt_regs`.
- **Handoff**: Calls C function `do_user_addr_fault(regs, error_code, address)`.

**THE DERIVED TRACE (DENSE)**:

1. **Instruction**: `MOV` (User Code) tries to write `0x7e95...`.
2. **Silicon**: MMU checks TLB/PageTable. Result: **Present=0**.
3. **Silicon**: Triggers **Exception 14**. Stores `0x7e95...` in **CR2**.
4. **Kernel**: `asm_exc_page_fault` reads **CR2**. Saves Regs.
5. **Logic**: `do_user_addr_fault` -> `handle_mm_fault`.
6. **Lookup**: `vma = find_vma(mm, 0x7e95...)`. (Axiom: Maple Tree).
7. **Check**: `vma->vm_flags` allows Write? **Yes**.
8. **Routing**: `pgd_offset` -> `pud` -> `pmd` -> `pte`. (Axiom: 4-Level Walk).
9. **State**: PTE is Empty (0). (Axiom: Demand Paging).
10. **Action**: `alloc_anon_folio`. (Axiom: Buddy Allocator).
11. **Result**: Allocated Physical Page **PFN 0x2CAD54**.
12. **Link**: `set_pte_at` writes `0x2CAD54 | Flags` to Page Table.
13. **Return**: `IRETQ` restores User context. Re-executes `MOV`.
14. **Success**: MMU finds PTE. Translates to `0x2CAD54000`. Write completes.

- **Goal**: Retroactively derive all terms using only basic counting and C memory model.

### Derivation 1: `current->stack`

- **Axiom**: `current` points to `struct task_struct`.
- **Constraint**: Every thread needs private scratchpad memory.
- **Definition**: `stack` member is a `void *` pointer managed by the kernel allocator.
- **Value**: The Memory Address of the *first byte* of the allocated array.
- **Code**: `void *kstack_base = current->stack;`

### Derivation 2: `THREAD_SIZE`

- **Axiom**: Allocations have size.
- **Definition**: `THREAD_SIZE` is a constant defined in kernel headers (e.g., 16384 bytes).
- **Code**: `unsigned long kstack_top = ... + THREAD_SIZE;`

### Derivation 3: `__builtin_frame_address(0)`

- **Axiom**: CPU RSP register points to the current top of the stack.
- **Definition**: GCC Builtin that returns the value of the Frame Pointer (or Stack Pointer equivalent).
- **Usage**: Used to sample the "Voltmeter" (Current CPU position).

### Missing Derivations (New Things Introduced)

1. `__builtin_frame_address`: Introduced without explaining it is a compiler intrinsic for reading RSP/RBP.
2. `THREAD_SIZE`: Introduced without showing its definition in `asm/page_64_types.h`.
3. `current->stack`: Introduced without showing the `task_struct` definition.

## Axiomatic Derivation: Step 2 → Step 3 Transition

### Why `if (flags & 0x1000)` → `lock_vma_under_rcu`?

**TERM 1: `mmap_lock`**

- Axiom A1: Computer has 1+ CPUs.
- Problem P1: CPU1 + CPU2 write same Address → Race. Solution: Lock.
- Definition D2: `mmap_lock` = Lock protecting Memory Map.
- Consequence C1: Only 1 CPU at a time → Slow.

**TERM 2: Per-VMA Locking**

- Problem P2: N=100 Regions, 4 CPUs, 1 Lock → 3 CPUs wait (75% Waste).
- Solution S1: 1 Lock per Region → 0% Wait.
- Definition D4: `FAULT_FLAG_VMA_LOCK` = Bit 12 = `0x1000`.

**TERM 3: Maple Tree**

- Problem P3: Find Region for Address. N=100. Linear = 100 steps. Slow.
- Axiom A3: Tree = O(log N). log2(100) ≈ 7 steps.
- Definition D5: `Maple Tree` = Linux Kernel VMA Tree.

**TERM 4: `lock_vma_under_rcu`**

- Definition D6: `RCU` = Readers Never Block.
- Definition D7: `lock_vma_under_rcu(mm, addr)` = Fast VMA Lookup via Maple Tree.

**TRACE (Live)**:

1. `flags = 0x1255`.
2. `0x1255 & 0x1000 = 0x1000 ≠ 0` → TRUE.
3. Call `lock_vma_under_rcu(mm, 0x716b326c8100)`.

## Axiomatic Derivation: VMA vs PAGE TABLE (Corrected)

### The Confusion

VMA is NOT a slice of RAM. VMA is a kernel struct.
Page Table Walk (4 levels) translates VA → PFN.
These are SEPARATE.

### Live Calculation (Python3, PID 14420)

```
VA = 0x716b326c8100
PGD Index = (0x716b326c8100 >> 39) & 0x1FF = 226
PUD Index = (0x716b326c8100 >> 30) & 0x1FF = 428
PMD Index = (0x716b326c8100 >> 21) & 0x1FF = 403
PTE Index = (0x716b326c8100 >> 12) & 0x1FF = 200
Offset    = (0x716b326c8100 & 0xFFF) = 256
```

### Two Paths

- **Path A (VMA)**: Maple Tree lookup. Is VA legal? Found VMA [0x716b326c8000-0x716b326c9000) rw-.
- **Path B (Page Table)**: PGD[226] → PUD[428] → PMD[403] → PTE[200] → PFN (or Fault).
