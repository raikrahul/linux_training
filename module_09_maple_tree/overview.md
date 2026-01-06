# Module 9: Maple Tree & VMA

## Overview

This module explores the maple tree data structure and Virtual Memory Areas (VMAs). You will understand how the kernel efficiently manages process address spaces.

---

## 1. Virtual Memory Area (VMA)

### What is a VMA?

A VMA describes a contiguous region of virtual memory:

```c
// include/linux/mm_types.h
struct vm_area_struct {
    unsigned long vm_start;       // Start address (inclusive)
    unsigned long vm_end;         // End address (exclusive)
    
    struct mm_struct *vm_mm;      // Owning address space
    
    pgprot_t vm_page_prot;        // Page protection
    unsigned long vm_flags;       // Flags (VM_READ, VM_WRITE, etc.)
    
    struct file *vm_file;         // Mapped file (or NULL)
    unsigned long vm_pgoff;       // File offset in pages
    
    const struct vm_operations_struct *vm_ops;
    
    // ... more fields
};
```

### VMA Flags

```c
// include/linux/mm.h
#define VM_READ         0x00000001    // Readable
#define VM_WRITE        0x00000002    // Writable
#define VM_EXEC         0x00000004    // Executable
#define VM_SHARED       0x00000008    // Shared mapping
#define VM_GROWSDOWN    0x00000100    // Stack (grows down)
#define VM_GROWSUP      0x00000200    // Heap-like (grows up)
#define VM_DONTCOPY     0x00020000    // Don't copy on fork
#define VM_LOCKED       0x00002000    // mlocked
```

---

## 2. Process Address Space

### Layout

```
High Address
┌─────────────────────────────────────────────────────────────┐
│  Kernel Space (not accessible to user)                      │
├─────────────────────────────────────────────────────────────┤ 0x7FFFFFFFF000
│  [VMA] Stack                                                 │
│  vm_start: 0x7FFF00000000                                    │
│  vm_end:   0x7FFF00100000                                    │
│  vm_flags: VM_READ | VM_WRITE | VM_GROWSDOWN                 │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  (unmapped gap)                                              │
│                                                              │
├─────────────────────────────────────────────────────────────┤
│  [VMA] Shared Library (libc.so)                              │
│  vm_start: 0x7F0000000000                                    │
│  vm_end:   0x7F0000200000                                    │
│  vm_flags: VM_READ | VM_EXEC                                 │
│  vm_file:  /lib/x86_64-linux-gnu/libc.so.6                   │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  (unmapped gap)                                              │
│                                                              │
├─────────────────────────────────────────────────────────────┤
│  [VMA] Heap (brk area)                                       │
│  vm_start: 0x00600000                                        │
│  vm_end:   0x00700000                                        │
│  vm_flags: VM_READ | VM_WRITE                                │
├─────────────────────────────────────────────────────────────┤
│  [VMA] Data segment                                          │
│  vm_start: 0x00500000                                        │
│  vm_end:   0x00600000                                        │
│  vm_flags: VM_READ | VM_WRITE                                │
├─────────────────────────────────────────────────────────────┤
│  [VMA] Text segment (code)                                   │
│  vm_start: 0x00400000                                        │
│  vm_end:   0x00500000                                        │
│  vm_flags: VM_READ | VM_EXEC                                 │
│  vm_file:  /path/to/executable                               │
└─────────────────────────────────────────────────────────────┘
Low Address
```

---

## 3. Maple Tree Data Structure

### Why Maple Tree?

Linux 6.1 replaced red-black trees with maple trees for VMA management:

| Feature | Red-Black Tree | Maple Tree |
|---------|----------------|------------|
| Lookup | O(log n) | O(log n) |
| RCU-safe | No | Yes |
| Cache efficiency | Poor | Good |
| Range ops | Slow | Fast |

### Structure

```
Maple Tree Node:
┌───────────────────────────────────────────────────────────────┐
│  Node type: range64                                           │
│                                                               │
│  Pivots: [addr1] [addr2] [addr3] ... [addrN]                 │
│                                                               │
│  Slots:  [ptr0] [ptr1] [ptr2] [ptr3] ... [ptrN+1]            │
│           │      │      │      │            │                │
│           ▼      ▼      ▼      ▼            ▼                │
│         <addr1  ≤addr2 ≤addr3 ≤addr4  ... >addrN             │
└───────────────────────────────────────────────────────────────┘
```

---

## 4. VMA Lookup

### find_vma()

```c
// mm/mmap.c
struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr)
{
    struct vm_area_struct *vma;
    
    // Use the maple tree iterator
    MA_STATE(mas, &mm->mm_mt, addr, addr);
    
    vma = mas_walk(&mas);
    if (!vma)
        return NULL;
    
    // find_vma returns VMA containing addr, or next VMA after addr
    if (vma->vm_end > addr)
        return vma;
    
    return NULL;
}
```

### Usage

```c
// Kernel code to check address validity
struct vm_area_struct *vma = find_vma(current->mm, address);

if (!vma) {
    // Address is beyond all VMAs
    return -EFAULT;
}

if (address < vma->vm_start) {
    // Address is in a hole (between VMAs)
    return -EFAULT;
}

// Address is valid, in this VMA
if (!(vma->vm_flags & VM_WRITE)) {
    // Not writable
    return -EACCES;
}
```

---

## 5. Reading VMAs from Userspace

### /proc/PID/maps

```c
// read_maps.c
#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *f = fopen("/proc/self/maps", "r");
    char line[512];
    
    printf("%-18s %-18s %-5s %-8s %-10s %s\n",
           "Start", "End", "Perm", "Offset", "Inode", "Path");
    
    while (fgets(line, sizeof(line), f)) {
        unsigned long start, end, offset, inode;
        char perms[5], dev[10], path[256] = "";
        
        sscanf(line, "%lx-%lx %4s %lx %9s %lu %255[^\n]",
               &start, &end, perms, &offset, dev, &inode, path);
        
        printf("0x%016lx 0x%016lx %s %08lx %10lu %s\n",
               start, end, perms, offset, inode, path);
    }
    
    fclose(f);
    return 0;
}
```

Output:
```
Start              End                Perm  Offset   Inode      Path
0x0000000000400000 0x0000000000401000 r--p  00000000      12345 /path/to/program
0x0000000000401000 0x0000000000402000 r-xp  00001000      12345 /path/to/program
0x00007ffff7d80000 0x00007ffff7f00000 r-xp  00000000      67890 /lib/libc.so.6
0x00007ffffffde000 0x00007ffffffff000 rw-p  00000000          0 [stack]
```

---

## 6. Kernel Module: VMA Walker

```c
// vma_walker.c
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/sched.h>

static void print_vmas(struct task_struct *task)
{
    struct mm_struct *mm = task->mm;
    struct vm_area_struct *vma;
    VMA_ITERATOR(vmi, mm, 0);
    
    if (!mm)
        return;
    
    mmap_read_lock(mm);
    
    pr_info("VMAs for PID %d (%s):\n", task->pid, task->comm);
    
    for_each_vma(vmi, vma) {
        pr_info("  [%016lx-%016lx] %c%c%c%c pgoff=%lu",
                vma->vm_start, vma->vm_end,
                vma->vm_flags & VM_READ  ? 'r' : '-',
                vma->vm_flags & VM_WRITE ? 'w' : '-',
                vma->vm_flags & VM_EXEC  ? 'x' : '-',
                vma->vm_flags & VM_SHARED ? 's' : 'p',
                vma->vm_pgoff);
        
        if (vma->vm_file) {
            pr_cont(" file=%pD", vma->vm_file);
        }
        pr_cont("\n");
    }
    
    mmap_read_unlock(mm);
}

static int __init vma_walker_init(void)
{
    print_vmas(current);
    return 0;
}

static void __exit vma_walker_exit(void) {}

module_init(vma_walker_init);
module_exit(vma_walker_exit);
MODULE_LICENSE("GPL");
```

---

## 7. VMA Operations

### mmap Creates VMA

```c
// User calls mmap()
void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

// Kernel creates VMA:
// vma->vm_start = (assigned address)
// vma->vm_end   = vm_start + 4096
// vma->vm_flags = VM_READ | VM_WRITE
// vma->vm_file  = NULL (anonymous)
```

### munmap Removes VMA

```c
munmap(p, 4096);
// VMA is removed from maple tree
// If partial unmap, VMA may be split
```

### mprotect Modifies VMA

```c
mprotect(p, 4096, PROT_READ);
// vma->vm_flags changed from VM_READ|VM_WRITE to VM_READ
// May split VMA if protecting only part
```

---

## 8. Practice Exercises

### Exercise 1: VMA Counter

Write a program that counts VMAs in /proc/self/maps by type:
- Anonymous
- File-backed
- Stack
- Heap

### Exercise 2: Maple Tree Depth

Create a kernel module that calculates the depth of the maple tree for current process.

### Exercise 3: VMA Growth Tracking

Monitor how VMA count changes as you:
- malloc() increasingly larger buffers
- mmap() multiple files
- Create threads

---

## Next Module

[Module 10: NUMA & Zones →](../module_10_numa_zones/)

[← Back to Course Index](../index.md)
