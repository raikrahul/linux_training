# AXIOMATIC PROOF: Page Fault at Offset (With Real Machine Data)

**Machine:** Linux 6.14.0-37-generic, x86_64
**Date:** 2026-01-02
**Real VA:** `0x73b34709f100`
**Real PFN:** `0x1189cb`
**Real Content:** `"offset world"`

---

# PART 1: AXIOMS (Definitions Before Use)

## AXIOM 1: The Byte
A Byte is 8 bits. Each bit is 0 or 1.

## AXIOM 2: The Address
An Address is a 64-bit integer. It names a Byte location in memory.

## AXIOM 3: The Page
A Page is 4096 consecutive Bytes.
```
4096 = 2^12
```
*   12 bits can count 0 to 4095.
*   Every Page starts at an address ending in `0x...000`.

## AXIOM 4: PAGE_SHIFT = 12
Source: `/usr/src/linux-headers-6.14.0-37-generic/include/vdso/page.h:13`
```c
#define PAGE_SHIFT CONFIG_PAGE_SHIFT  // CONFIG_PAGE_SHIFT = 12 on x86_64
```

## AXIOM 5: PAGE_MASK = 0xFFFFFFFFFFFFF000
Source: `/usr/src/linux-headers-6.14.0-37-generic/include/vdso/page.h:28`
```c
#define PAGE_MASK (~(PAGE_SIZE - 1))
```
Derivation:
```
PAGE_SIZE = 4096 = 0x1000
PAGE_SIZE - 1 = 4095 = 0x0FFF
~0x0FFF = 0xFFFFFFFFFFFFF000
```
PAGE_MASK has bottom 12 bits = 0, all other bits = 1.

## AXIOM 6: Virtual Page Number (VPN)
Given Address A:
```
VPN = A >> PAGE_SHIFT = A >> 12
```
VPN tells which Page the Address belongs to.

## AXIOM 7: Page Offset
Given Address A:
```
Offset = A & 0xFFF
```
Offset tells which Byte within the Page (0 to 4095).

## AXIOM 8: CR2 Register
CR2 is a hardware register inside the x86 CPU.
When a Page Fault occurs, CPU writes the faulting Address into CR2.
Source: `/usr/src/linux-headers-6.14.0-37-generic/arch/x86/include/asm/special_insns.h:33-38`
```c
static __always_inline unsigned long native_read_cr2(void)
{
    unsigned long val;
    asm volatile("mov %%cr2,%0\n\t" : "=r" (val) : __FORCE_ORDER);
    return val;
}
```

**GCC Inline Assembly Syntax:**
```
asm volatile( "TEMPLATE" : OUTPUTS : INPUTS : CLOBBERS );
```
| Part | Value | Meaning |
|------|-------|---------|
| `mov` | Instruction | Copy data. |
| `%%cr2` | Source | CPU register CR2 (double `%` escapes). |
| `%0` | Destination | Placeholder for operand #0. |
| `"=r"` | Constraint | Output to any register. |
| `(val)` | C Variable | Bind to variable `val`. |

**Placeholder `%0`:** GCC replaces `%0` with a real register (e.g., `%rax`).

## AXIOM 9: Per-CPU Storage (pcpu_hot)
Each CPU has its own private `pcpu_hot` struct.
Source: `/usr/src/linux-headers-6.14.0-37-generic/arch/x86/include/asm/current.h:15-18,38`
```c
struct pcpu_hot {
    struct task_struct  *current_task;
    int                 preempt_count;
    int                 cpu_number;
};
DECLARE_PER_CPU_ALIGNED(struct pcpu_hot, pcpu_hot);
```

## AXIOM 10: The `current` Macro
Source: `/usr/src/linux-headers-6.14.0-37-generic/arch/x86/include/asm/current.h:44-52`
```c
static __always_inline struct task_struct *get_current(void)
{
    return this_cpu_read_stable(pcpu_hot.current_task);
}
#define current get_current()
```
`current` = The `task_struct` of the process running on THIS CPU.

## AXIOM 11: Interrupt Does NOT Change `current`
When Page Fault fires:
1.  CPU pushes registers to stack.
2.  CPU jumps to kernel handler.
3.  `pcpu_hot.current_task` is NOT modified.
4.  ∴ `current` still points to the faulting process.

## AXIOM 12: PAGE_OFFSET (Direct Map Base)
Source: Live machine data from `dmesg`:
```
AXIOM: PAGE_OFFSET = 0xffff8bfc40000000
```
Kernel Virtual Address = Physical Address + PAGE_OFFSET.

---

# PART 2: TRACE WITH REAL DATA

## STEP 1: mmap Returns Base Address
**Source:** `mm_exercise_user.c` via `mmap()`.
**Real Data (from dmesg):**
```
VA: 0x73b34709f000  (Base, ends in 000)
```
*   Bottom 12 bits = `0x000`.
*   ∴ Base is Page-Aligned.

## STEP 2: User Adds Offset
**Source:** `mm_exercise_user.c` line 97:
```c
char *target_addr = (char *)vaddr + 0x100;
```
**Calculation:**
```
Target = Base + 0x100
Target = 0x73b34709f000 + 0x100
Target = 0x73b34709f100
```

## STEP 3: Calculate VPN of Target
```
VPN = Target >> 12
VPN = 0x73b34709f100 >> 12
VPN = 0x73b34709f
```
(This is the Page Number, a very large integer.)

## STEP 4: Calculate Offset of Target
```
Offset = Target & 0xFFF
Offset = 0x73b34709f100 & 0xFFF
Offset = 0x100
```
*   Proof: The last 3 hex digits of `0x73b34709f100` are `100`.

---

## STEP 5: CPU Receives Instruction
Userspace code: `strcpy(target_addr, "offset world");`
CPU must write to Virtual Address `0x73b34709f100`.

## STEP 6: CPU Calculates Page Table Indices
**Real Data (from dmesg):**
```
RAW MATH: PGD=231(0xe7) PUD=205(0xcd) PMD=56(0x38) PTE=159(0x9f)
```
These indices are calculated from the VA bits:
```
PGD Index = (VA >> 39) & 0x1FF = 231
PUD Index = (VA >> 30) & 0x1FF = 205
PMD Index = (VA >> 21) & 0x1FF = 56
PTE Index = (VA >> 12) & 0x1FF = 159
```

## STEP 7: CPU Checks Page Table
If PTE for this page is MISSING → Page Fault.
**Assumption:** First access, so PTE is missing.

## STEP 8: CPU Raises Exception #14
x86 Exception 14 = Page Fault (hardcoded in CPU silicon).

## STEP 9: CPU Writes Target Address to CR2
```
CR2 = 0x73b34709f100
```
This happens automatically in hardware before the handler runs.

## STEP 10: CPU Jumps to Kernel Handler
Entry point: architecture-specific fault handler.

---

## STEP 11: Kernel Reads CR2
```c
address = read_cr2();  // Returns 0x73b34709f100
```

## STEP 12: Kernel Calculates Page Base
```
Page Base = CR2 & PAGE_MASK
Page Base = 0x73b34709f100 & 0xFFFFFFFFFFFFF000
Page Base = 0x73b34709f000
```

## STEP 13: Kernel Finds `current`
```c
struct task_struct *tsk = current;  // Points to mm_exercise_user process
struct mm_struct *mm = tsk->mm;     // The memory descriptor
```
(See AXIOM 10, 11)

## STEP 14: Kernel Finds VMA
Kernel searches `mm->mmap` tree for VMA containing `0x73b34709f000`.
Found: The anonymous VMA created by `mmap()`.

## STEP 15: Kernel Allocates Physical Page
Buddy allocator returns a free page.
**Real Data (from dmesg):**
```
PFN = 0x1189cb
```

## STEP 16: Kernel Calculates Physical Base
```
Physical Base = PFN << 12
Physical Base = 0x1189cb << 12
Physical Base = 0x1189cb000
```

## STEP 17: Kernel Writes PTE
```
PTE = Physical_Base | FLAGS
PTE = 0x1189cb000 | 0x867
```
**Real Data (from dmesg):**
```
L4 PTE: Val = 0x80000001189cb867
```

## STEP 18: Kernel Returns (IRET)
Control returns to userspace. CPU retries the instruction.

---

## STEP 19: CPU Retries Write to 0x73b34709f100

## STEP 20: CPU Walks Page Table Again
PTE now EXISTS. Value = `0x80000001189cb867`.

## STEP 21: CPU Extracts PFN from PTE
```
PFN = (PTE & 0x000FFFFFFFFFF000) >> 12
PFN = (0x80000001189cb867 & 0x000FFFFFFFFFF000) >> 12
PFN = 0x1189cb
```

## STEP 22: CPU Calculates Physical Address
```
Physical = (PFN << 12) + Offset
Physical = 0x1189cb000 + 0x100
Physical = 0x1189cb100
```
**Real Data (from dmesg):**
```
L4 PTE: PFN = 0x1189cb | Offset = 0x100 | Phys = 0x1189cb100
```

## STEP 23: CPU Writes to Physical RAM
Hardware accesses RAM at byte `0x1189cb100`.
Writes: `"offset world"`.

## STEP 24: Kernel Reads It Back (Verification)
```c
void *kva = (void *)(0x1189cb100 + PAGE_OFFSET);
// kva = 0x1189cb100 + 0xffff8bfc40000000
// kva = 0xffff8bfd589cb100
memcpy(buf, kva, 16);  // Reads "offset world"
```
**Real Data (from dmesg):**
```
AXIOM: PAGE_OFFSET = 0xffff8bfc40000000
RAM CONTENT (first 16 bytes): "offset world"
```

---

# PART 3: SUMMARY TABLE

| Step | Value | Source |
|------|-------|--------|
| Base VA | `0x73b34709f000` | mmap return |
| Offset | `0x100` | User code |
| Target VA | `0x73b34709f100` | Base + Offset |
| PGD Index | 231 (0xe7) | `(VA >> 39) & 0x1FF` |
| PUD Index | 205 (0xcd) | `(VA >> 30) & 0x1FF` |
| PMD Index | 56 (0x38) | `(VA >> 21) & 0x1FF` |
| PTE Index | 159 (0x9f) | `(VA >> 12) & 0x1FF` |
| PFN | `0x1189cb` | Kernel allocator |
| Phys Base | `0x1189cb000` | PFN << 12 |
| Phys Addr | `0x1189cb100` | Phys Base + Offset |
| PAGE_OFFSET | `0xffff8bfc40000000` | dmesg |
| RAM Content | `"offset world"` | dmesg |

---

**TERMS INTRODUCED WITHOUT DERIVATION:** None.
All values sourced from live machine or kernel headers.
