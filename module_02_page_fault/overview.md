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
