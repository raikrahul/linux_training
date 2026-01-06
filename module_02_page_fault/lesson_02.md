# Module 2: Page Fault Handling

## Overview

This module provides an in-depth exploration of the Linux page fault handling mechanism. You will trace the complete path from CPU exception to page allocation.

---

## 1. What is a Page Fault?

A page fault occurs when the CPU tries to access a virtual address that:
- Has no valid page table entry (present bit = 0)
- Has permission violation (write to read-only page)
- Tries to access kernel memory from user mode

```
Program executes: MOV RAX, [0x7FFE12345678]
                        │
                        ▼
                  ┌───────────┐
                  │ MMU Check │
                  └─────┬─────┘
                        │
        ┌───────────────┴───────────────┐
        │                               │
  PTE Present?                    PTE Absent?
        │                               │
        ▼                               ▼
   Access OK               CPU raises exception #14
        │                               │
        ▼                               ▼
   Return data              Jump to page fault handler
```

---

## 2. CPU Exception Delivery

When a page fault occurs, the CPU:

```
1. Push SS (stack segment)
2. Push RSP (stack pointer)
3. Push RFLAGS (flags)
4. Push CS (code segment)
5. Push RIP (instruction pointer)
6. Push error_code (fault information)
7. Load CR2 with faulting address
8. Jump to IDT[14] (page fault handler)
```

### Error Code Format

```
Error Code (32 bits):
┌────┬────┬────┬────┬────┬────┬─────────────────────┐
│ 31 │ ...│  4 │  3 │  2 │  1 │  0                  │
├────┼────┼────┼────┼────┼────┼─────────────────────┤
│ 0  │ 0  │ I  │RSVD│ U  │ W  │ P                   │
└────┴────┴────┴────┴────┴────┴─────────────────────┘

P (bit 0): 1 = protection violation, 0 = page not present
W (bit 1): 1 = write access, 0 = read access
U (bit 2): 1 = user mode, 0 = kernel mode
RSVD (bit 3): 1 = reserved bit set in PTE
I (bit 4): 1 = instruction fetch
```

### Example: First Write to malloc'd Memory

```
malloc(4096) returns 0x5555555AA000
ptr[0] = 'A';  // First write

Error code = 0x6 = 0b0110
  P=0: Page was NOT present (demand paging)
  W=1: Write access
  U=1: User mode
  RSVD=0: No reserved bit violation
  I=0: Not instruction fetch

CR2 = 0x5555555AA000 (faulting address)
```

---

## 3. Kernel Fault Handler Chain

```
exc_page_fault()               [arch/x86/mm/fault.c]
         │
         ▼
do_user_addr_fault()           [arch/x86/mm/fault.c]
         │
         │ Lock mm->mmap_lock
         │ Find VMA containing address
         ▼
handle_mm_fault()              [mm/memory.c]
         │
         │ Walk/allocate page tables
         ▼
__handle_mm_fault()
         │
         │ Get PMD, handle huge pages
         ▼
handle_pte_fault()             [mm/memory.c]
         │
         ├──► do_anonymous_page()    [First access to anon mem]
         ├──► do_fault()             [File-backed page]
         ├──► do_swap_page()         [Page in swap]
         └──► do_wp_page()           [Copy-on-write]
```

---

## 4. Kernel Code Walkthrough

### Entry Point: exc_page_fault

```c
// arch/x86/mm/fault.c
DEFINE_IDTENTRY_RAW_ERRORCODE(exc_page_fault)
{
    unsigned long address = read_cr2();  // Get faulting address
    
    // ... error handling ...
    
    handle_page_fault(regs, error_code, address);
}

static void handle_page_fault(struct pt_regs *regs,
                              unsigned long error_code,
                              unsigned long address)
{
    // Kernel address? Handle differently
    if (unlikely(fault_in_kernel_space(address))) {
        do_kern_addr_fault(regs, error_code, address);
        return;
    }
    
    // User address
    do_user_addr_fault(regs, error_code, address);
}
```

### User Address Fault Handler

```c
// arch/x86/mm/fault.c
static void do_user_addr_fault(struct pt_regs *regs,
                               unsigned long error_code,
                               unsigned long address)
{
    struct mm_struct *mm = current->mm;
    struct vm_area_struct *vma;
    vm_fault_t fault;
    
    // Lock the address space
    mmap_read_lock(mm);
    
    // Find VMA containing the faulting address
    vma = find_vma(mm, address);
    if (!vma || address < vma->vm_start) {
        // No VMA found = SIGSEGV
        bad_area(regs, error_code, address);
        return;
    }
    
    // Check permissions
    if (!access_allowed(vma, error_code)) {
        bad_area_access_error(regs, error_code, address);
        return;
    }
    
    // Handle the fault
    fault = handle_mm_fault(vma, address, flags, regs);
    
    mmap_read_unlock(mm);
}
```

### handle_mm_fault: Page Table Walk

```c
// mm/memory.c
vm_fault_t handle_mm_fault(struct vm_area_struct *vma,
                           unsigned long address,
                           unsigned int flags,
                           struct pt_regs *regs)
{
    struct vm_fault vmf = {
        .vma = vma,
        .address = address & PAGE_MASK,  // Page-aligned
        .flags = flags,
        .pgoff = linear_page_index(vma, address),
    };
    
    return __handle_mm_fault(vma, address, flags);
}

static vm_fault_t __handle_mm_fault(struct vm_area_struct *vma,
                                    unsigned long address,
                                    unsigned int flags)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    
    // Get or allocate each level
    pgd = pgd_offset(mm, address);
    p4d = p4d_alloc(mm, pgd, address);
    pud = pud_alloc(mm, p4d, address);
    pmd = pmd_alloc(mm, pud, address);
    
    // Handle PTE-level fault
    return handle_pte_fault(&vmf);
}
```

---

## 5. Fault Types

### Type 1: Anonymous Page Fault (Demand Paging)

```
malloc(4096) → returns ptr
ptr[0] = 'A' → PAGE FAULT

do_anonymous_page():
1. Allocate physical page via alloc_page()
2. Clear the page (zero-fill)
3. Create PTE pointing to new page
4. Return to userspace
```

```c
// mm/memory.c
static vm_fault_t do_anonymous_page(struct vm_fault *vmf)
{
    struct page *page;
    pte_t entry;
    
    // Allocate a new zeroed page
    page = alloc_zeroed_user_highpage(vma, vmf->address);
    if (!page)
        return VM_FAULT_OOM;
    
    // Increment page reference count
    get_page(page);
    
    // Create PTE entry
    entry = mk_pte(page, vma->vm_page_prot);
    if (vma->vm_flags & VM_WRITE)
        entry = pte_mkwrite(entry);
    
    // Install PTE
    set_pte_at(mm, vmf->address, vmf->pte, entry);
    
    return 0;
}
```

### Type 2: Copy-on-Write Fault

```
fork() creates child with same page tables
Parent has PTE: PA=0x12345000, R/W, refcount=2

Child writes to page → PAGE FAULT (write to read-only)

do_wp_page():
1. Check if page is shared (refcount > 1)
2. Allocate new physical page
3. Copy contents from old page
4. Update child's PTE to point to new page
5. Mark new PTE as writable
```

```c
// mm/memory.c
static vm_fault_t do_wp_page(struct vm_fault *vmf)
{
    struct page *old_page = vmf->page;
    struct page *new_page;
    
    // Is page shared?
    if (page_count(old_page) > 1) {
        // Must copy
        new_page = alloc_page(GFP_HIGHUSER);
        copy_user_highpage(new_page, old_page, vmf->address);
        
        // Update PTE to new page
        entry = mk_pte(new_page, vma->vm_page_prot);
        entry = pte_mkwrite(entry);
        set_pte_at(mm, vmf->address, vmf->pte, entry);
        
        // Release reference to old page
        put_page(old_page);
    } else {
        // Exclusive access, just make writable
        entry = pte_mkwrite(vmf->orig_pte);
        set_pte_at(mm, vmf->address, vmf->pte, entry);
    }
    
    return 0;
}
```

---

## 6. Tracing with Kprobe

### Kprobe Module for Page Fault

```c
// fault_trace.c
#include <linux/module.h>
#include <linux/kprobes.h>

static struct kprobe kp = {
    .symbol_name = "handle_mm_fault",
};

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct vm_area_struct *vma = (void *)regs->di;
    unsigned long address = regs->si;
    unsigned int flags = regs->dx;
    
    if (strcmp(current->comm, "my_program") == 0) {
        pr_info("[FAULT] PID=%d addr=0x%lx flags=0x%x "
                "vma=[0x%lx-0x%lx]\n",
                current->pid, address, flags,
                vma->vm_start, vma->vm_end);
    }
    return 0;
}

static int __init fault_trace_init(void)
{
    kp.pre_handler = handler_pre;
    return register_kprobe(&kp);
}
module_init(fault_trace_init);

static void __exit fault_trace_exit(void)
{
    unregister_kprobe(&kp);
}
module_exit(fault_trace_exit);

MODULE_LICENSE("GPL");
```

### Sample dmesg Output

```
[FAULT] PID=1234 addr=0x7f8a12340000 flags=0x255 vma=[0x7f8a12340000-0x7f8a12440000]
[FAULT] PID=1234 addr=0x7ffd12345000 flags=0x255 vma=[0x7ffd12300000-0x7ffd12400000]
```

---

## 7. Practice Exercises

### Exercise 1: Decode Error Codes

Given these error codes, determine fault cause:
- error_code = 0x0 → ?
- error_code = 0x2 → ?
- error_code = 0x7 → ?
- error_code = 0x15 → ?

### Exercise 2: Force Different Fault Types

Write a C program that triggers:
1. Demand paging fault (access malloc'd memory)
2. Copy-on-write fault (fork + write)
3. Protection fault (write to mmap'd read-only)

### Exercise 3: Write a Fault Counter

Create a kprobe that counts page faults per process.

---

## Next Module

[Module 3: Memory Allocators →](../module_03_allocators/)

[← Back to Course Index](../index.md)

---

## AXIOMATIC EXERCISES — BRUTE FORCE CALCULATION

### EXERCISE A: ERROR CODE DECODE

```
GIVEN: error_code = 0x17

TASK: Decode each bit

1. 0x17 in binary = ___ ___ ___ ___ ___ (5 bits)
2. bit[0] (P) = ___ → page present? ___
3. bit[1] (W) = ___ → write access? ___
4. bit[2] (U) = ___ → user mode? ___
5. bit[3] (RSVD) = ___ → reserved bit violation? ___
6. bit[4] (I) = ___ → instruction fetch? ___

DESCRIBE FAULT: ________________________________
```

### EXERCISE B: FAULT ADDRESS FROM CR2

```
GIVEN:
  CR2 = 0x7FFE_1234_5678
  VMA: vm_start=0x7FFE_0000_0000, vm_end=0x7FFF_0000_0000

TASK:

1. Is address in VMA? vm_start ≤ CR2 < vm_end → ___ ≤ ___ < ___ → YES/NO
2. Offset into VMA = CR2 - vm_start = ___ - ___ = ___
3. Page offset = CR2 & 0xFFF = ___
4. Page number within VMA = offset / 4096 = ___
```

### EXERCISE C: HANDLER CALL CHAIN

```
GIVEN: User writes to address 0x5555_5678_9000, first access, anonymous VMA

TASK: Fill call chain

1. CPU exception → exc_page_fault(regs, error_code=___)
2. error_code bits: P=___ W=___ U=___ → demand paging / COW / protection?
3. → do_user_addr_fault() → find_vma(mm, 0x5555_5678_9000) → VMA found?
4. → handle_mm_fault(vma, addr, flags) → __handle_mm_fault()
5. → handle_pte_fault() → PTE present? NO → do_anonymous_page() / do_fault()?
6. → alloc_page(GFP_HIGHUSER) → returns struct page at ___
7. → mk_pte(page, vm_page_prot) → creates PTE = ___
8. → set_pte_at() → installs PTE in page table
```

### EXERCISE D: COPY-ON-WRITE CALCULATION

```
GIVEN:
  Parent PTE[100] = 0x12345_003 (PA=0x12345000, flags=003=present+write)
  fork() creates child, marks PTEs read-only
  Child writes to page

TASK:

1. After fork, Parent PTE[100] = 0x12345_001 (write bit cleared) ✓
2. After fork, Child PTE[100] = 0x12345_001 (same PA, read-only) ✓
3. page->_refcount = 2 (shared between parent and child)
4. Child writes → error_code = ___ (P=1, W=1, U=1) = 0x___
5. do_wp_page() checks: page_count(page) = ___ → must copy? YES/NO
6. New page allocated at PA = 0xABCDE000
7. copy_user_highpage(new, old) → copies 4096 bytes
8. Child PTE[100] = 0xABCDE_003 (new PA, writable)
9. Old page->_refcount = ___ (decremented)
```

### EXERCISE E: KPROBE ARGUMENT EXTRACTION

```
GIVEN: kprobe on handle_mm_fault
  handle_mm_fault(struct vm_area_struct *vma, unsigned long addr, unsigned int flags, struct pt_regs *regs)

x86_64 ABI:
  arg1 = RDI, arg2 = RSI, arg3 = RDX, arg4 = RCX

TASK: Map registers to arguments

1. vma pointer = regs->___ = (struct vm_area_struct *)regs->___
2. address = regs->___ = ___
3. flags = regs->___ = ___
4. pt_regs = regs->___ = ___

GIVEN: regs->di = 0xFFFF8881_12340000, regs->si = 0x7FFE_5678_9000

5. vma = ___
6. faulting address = ___
```

---

## FAILURE PREDICTIONS

```
FAILURE 1: error_code bit order wrong → misidentify fault type
FAILURE 2: Forgetting vm_end is exclusive → incorrectly say address not in VMA
FAILURE 3: Confusing P=0 (not present) with P=1 (protection fault)
FAILURE 4: x86_64 ABI: arg order RDI,RSI,RDX,RCX,R8,R9 → not RAX,RBX,RCX
FAILURE 5: COW page shared → refcount > 1 → must copy, not just make writable
FAILURE 6: After fork, PTEs point to SAME physical page, not copied
```

---

## W-QUESTIONS — NUMERICAL ANSWERS

### WHAT: Error Code
```
error_code = 0x7 = 0b00111
bit[0]=1 → page present (protection fault, not absent)
bit[1]=1 → write access attempted
bit[2]=1 → user mode
∴ User tried to write to present read-only page → COW fault
```

### WHY: Not Map at malloc
```
malloc(1GB) → returns VA 0x7F0000000000
Pages allocated at malloc? 0 pages
First write triggers fault → 1 page allocated
1GB / 4KB = 262144 page faults if fully used
Lazy allocation saves: 262144 × 4KB = 1GB RAM if never touched
```

### WHERE: Fault Handler Lives
```
IDT[14] → exc_page_fault at 0xFFFFFFFF812A0000
CR2 loaded with faulting address by CPU
Kernel stack at 0xFFFF888100001000
Handler reads CR2: asm("mov %%cr2, %0" : "=r"(addr))
```

### WHO: Triggers Fault
```
Process PID=1234 with mm→pgd at 0x12340000
VMA at [0x7F0000000000, 0x7F0000100000)
Instruction at RIP=0x401234 does: MOV [0x7F0000050000], RAX
PTE for 0x7F0000050000 = 0 (not present)
→ CPU raises fault, kernel handles for PID 1234
```

### WHEN: Different Fault Types
```
T₁: malloc(4096), ptr=0x555555555000, no fault
T₂: ptr[0] = 'A' → fault, error_code=0x6 (P=0,W=1,U=1), do_anonymous_page()
T₃: fork(), child PTE marked read-only
T₄: child writes ptr[0] = 'B' → fault, error_code=0x7 (P=1,W=1,U=1), do_wp_page()
```

### WITHOUT: No Demand Paging
```
Process needs 1GB heap
Without demand paging: allocate 1GB immediately
  = 262144 pages × alloc_page() = 262144 calls
  = 262144 × 4096 bytes zeroed
Time: 262144 × 500ns = 131ms at startup

With demand paging: 0 pages at malloc, fault as needed
Startup time: ~0ms
Only pay for pages actually touched
```

### WHICH: Handler Path
```
error_code & 1 = 0 → not present → do_anonymous_page() OR do_fault()
error_code & 1 = 1 → present → do_wp_page() (COW)
error_code & 2 = 0 → read fault
error_code & 2 = 2 → write fault
error_code & 4 = 0 → kernel mode
error_code & 4 = 4 → user mode
```

---

## ANNOYING CALCULATIONS — BREAKDOWN

### Annoying: Error Code Bits
```
error_code = 0x15 = 0b10101
bit0 = 1 → P=1 (present)
bit1 = 0 → W=0 (read)
bit2 = 1 → U=1 (user)
bit3 = 0 → RSVD=0
bit4 = 1 → I=1 (instruction fetch)
∴ User tried to execute from present non-executable page
```

### Annoying: VMA Boundary Check
```
CR2 = 0x7FFE_FFFF_FFFF
VMA: vm_start=0x7FFE_0000_0000, vm_end=0x7FFF_0000_0000
Check: 0x7FFE_0000_0000 ≤ 0x7FFE_FFFF_FFFF < 0x7FFF_0000_0000
       0x7FFE_0000_0000 ≤ 0x7FFE_FFFF_FFFF ✓
       0x7FFE_FFFF_FFFF < 0x7FFF_0000_0000 ✓
∴ Address IS in VMA
```

### Annoying: Page Offset from VMA Start
```
CR2 = 0x7FFE_1234_5678
VMA starts at 0x7FFE_0000_0000
Offset = 0x7FFE_1234_5678 - 0x7FFE_0000_0000 = 0x1234_5678 = 305419896 bytes
Page number = 305419896 / 4096 = 74565 (floor)
Page offset = 305419896 % 4096 = 1656 bytes into page
```

### Annoying: Refcount After Fork
```
Before fork: page refcount = 1
After fork: parent refs + child refs = 1 + 1 = 2
mapcount: parent PTE + child PTE = 2 mappings
Child COW write:
  - new page refcount = 1
  - old page refcount = 2 - 1 = 1
```

---

## ATTACK PLAN

```
1. Decode error_code bit-by-bit: bit0=P, bit1=W, bit2=U, bit3=RSVD, bit4=I
2. Check VMA containment: start ≤ addr < end (end exclusive!)
3. Calculate page offset: (addr - vm_start) / 4096
4. On fork: refcount → refcount+1, mark PTEs read-only
5. On COW: allocate new page, copy 4096 bytes, update child PTE
```

---

## ADDITIONAL FAILURE PREDICTIONS

```
FAILURE 7: P=1 means protection fault, NOT that page allocation is needed
FAILURE 8: error_code=0x6 vs 0x7 → one bit difference changes entire path
FAILURE 9: VMA end is exclusive → addr=vm_end is OUTSIDE VMA
FAILURE 10: Must copy page data, not just update PTE → 4096 bytes moved
```
