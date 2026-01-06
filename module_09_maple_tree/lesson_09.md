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

---

## AXIOMATIC EXERCISES — BRUTE FORCE CALCULATION

### EXERCISE A: VMA ADDRESS CONTAINMENT

```
GIVEN:
  VMA: vm_start=0x7F00_0000_0000, vm_end=0x7F00_0001_0000

TASK: For each address, is it in VMA?

Rule: vm_start ≤ addr < vm_end

1. addr=0x7F00_0000_0000: ___ ≤ ___ < ___ → YES/NO
2. addr=0x7F00_0000_8000: ___ ≤ ___ < ___ → YES/NO
3. addr=0x7F00_0000_FFFF: ___ ≤ ___ < ___ → YES/NO
4. addr=0x7F00_0001_0000: ___ ≤ ___ < ___ → YES/NO (tricky!)
5. addr=0x7EFF_FFFF_FFFF: ___ ≤ ___ < ___ → YES/NO

TRICKY: vm_end is EXCLUSIVE
```

### EXERCISE B: VMA SIZE AND PAGE COUNT

```
GIVEN:
  vm_start = 0x5555_5678_0000
  vm_end   = 0x5555_5680_0000

TASK:

1. VMA size = vm_end - vm_start = 0x___ - 0x___ = 0x___ = ___ bytes
2. 0x___ bytes = ___ KB = ___ MB
3. Pages in VMA = size / 4096 = ___ / 4096 = ___
4. If vm_pgoff = 0x100, first file page = ___ (256 × 4096 from file start)
```

### EXERCISE C: FIND_VMA BEHAVIOR

```
GIVEN VMAs (sorted by start):
  VMA A: [0x1000, 0x2000)
  VMA B: [0x3000, 0x4000)
  VMA C: [0x5000, 0x6000)

TASK: What does find_vma(mm, addr) return?

find_vma returns VMA containing addr, OR first VMA after addr

1. find_vma(mm, 0x1500) → VMA ___ (contains 0x1500)
2. find_vma(mm, 0x2500) → VMA ___ (first after 0x2500)
3. find_vma(mm, 0x0500) → VMA ___ (first after 0x0500)
4. find_vma(mm, 0x6500) → ___ (no VMA after)
5. find_vma(mm, 0x3000) → VMA ___ (vm_start = addr)
```

### EXERCISE D: VMA FLAGS DECODE

```
GIVEN: vm_flags = 0x00100073

TASK: Extract permissions

Binary: 0000 0000 0001 0000 0000 0000 0111 0011

bit 0 (VM_READ)    = ___ → readable? ___
bit 1 (VM_WRITE)   = ___ → writable? ___
bit 2 (VM_EXEC)    = ___ → executable? ___
bit 3 (VM_SHARED)  = ___ → shared? ___
bit 8 (VM_GROWSDOWN) = ___ → stack? ___

Permission string: ___-__ (like "rwxp" or "r--s")
```

---

## FAILURE PREDICTIONS

```
FAILURE 1: vm_end is exclusive → addr=vm_end is NOT in VMA
FAILURE 2: find_vma returns next VMA if addr not contained → not NULL
FAILURE 3: Confusing vm_pgoff (in pages) with byte offset
FAILURE 4: VM_SHARED bit 3, not bit 4 → wrong flag extraction
FAILURE 5: Gaps between VMAs are UNMAPPED → access causes SIGSEGV
```

---

## W-QUESTIONS — NUMERICAL ANSWERS

### WHAT: VMA Structure
```
VMA at 0xFFFF888112340000:
  vm_start = 0x7F00_0000_0000
  vm_end = 0x7F00_0010_0000
  Size = 0x10_0000 = 1MB = 256 pages
  vm_flags = 0x73 = rwxp
  vm_file = 0xFFFF888198765000 (mapped file)
  vm_pgoff = 0x100 (file offset = 256 × 4KB = 1MB)
```

### WHY: Maple Tree Not RB-Tree
```
RB-tree lookup: follow left/right pointers
  Each step = 1 cache miss = 100ns
  Depth 20 = 2μs lookup

Maple tree: B-tree style, multiple entries per node
  Node fits in cache line (64 bytes)
  Depth 4 = 400ns lookup
  5× faster for large trees
```

### WHERE: VMA Stored
```
mm->mm_mt = maple tree root
Node at 0xFFFF888112350000:
  Pivots: [0x7F00_0000, 0x7F00_0100, 0x7F00_0200]
  Slots: [VMA0, VMA1, VMA2, NULL]
Address 0x7F00_0150:
  > pivot[0], < pivot[1] → slot[1] = VMA1
```

### WHO: Modifies VMAs
```
mmap() → allocate new VMA, insert into maple tree
munmap() → remove VMA from maple tree
mprotect() → may split VMA (1 VMA → 3 VMAs)
fork() → duplicate all VMAs for child
brk() → extend heap VMA's vm_end
```

### WHEN: VMA Lookup
```
Page fault at 0x7F00_0050_1234:
T₁: find_vma(mm, 0x7F00_0050_1234)
T₂: Maple tree walk: root → internal → leaf
T₃: Return VMA with start ≤ addr
T₄: Check addr < vm_end? YES → valid
T₅: Check vm_flags for WRITE → allowed?
```

### WITHOUT: No VMA
```
Every virtual address valid? No security!
Without VMA:
  - Cannot distinguish stack from heap
  - Cannot enforce read/write/exec permissions
  - Cannot track file mappings
Process isolation impossible
```

### WHICH: VMA Lookup Returns
```
find_vma(mm, addr) returns:
  - VMA containing addr (if vm_start ≤ addr < vm_end)
  - First VMA after addr (if in gap)
  - NULL (if addr beyond all VMAs)

Example: VMAs at [0x1000,0x2000), [0x3000,0x4000)
  find_vma(0x1500) → VMA1 (contains)
  find_vma(0x2500) → VMA2 (next after gap)
  find_vma(0x5000) → NULL
```

---

## ANNOYING CALCULATIONS — BREAKDOWN

### Annoying: VMA Page Count
```
vm_start = 0x7FFE_1234_0000
vm_end = 0x7FFE_1236_8000
size = 0x7FFE_1236_8000 - 0x7FFE_1234_0000 = 0x28000 = 163840 bytes
pages = 163840 / 4096 = 40 pages
```

### Annoying: Check Address in VMA
```
addr = 0x7FFE_1235_FFFF
vm_start = 0x7FFE_1234_0000
vm_end = 0x7FFE_1236_0000
Check: 0x7FFE_1234_0000 ≤ 0x7FFE_1235_FFFF < 0x7FFE_1236_0000
  0x7FFE_1234_0000 ≤ 0x7FFE_1235_FFFF ✓
  0x7FFE_1235_FFFF < 0x7FFE_1236_0000 ✓
∴ Address IS in VMA
```

### Annoying: Hole Detection
```
VMA1: [0x1000, 0x2000)
VMA2: [0x3000, 0x4000)
Hole at [0x2000, 0x3000)
Size = 0x3000 - 0x2000 = 0x1000 = 4KB = 1 page hole
Access 0x2500 → find_vma returns VMA2, but 0x2500 < 0x3000 → FAULT!
```

---

## ATTACK PLAN

```
1. find_vma(mm, addr) to get VMA
2. Check addr >= vma->vm_start
3. Check addr < vma->vm_end
4. Both true → address valid in VMA
5. Else →SegFault or hole
```

---

## ADDITIONAL FAILURE PREDICTIONS

```
FAILURE 7: find_vma returns NEXT vma, not NULL, if in gap
FAILURE 8: Must check both start AND end → vm_end is exclusive
FAILURE 9: vm_pgoff in pages, not bytes → multiply by 4096
FAILURE 10: mprotect may split VMA → original VMA pointer invalidated
```

---

## SHELL COMMANDS — PARADOXICAL THINKING EXERCISES

### COMMAND 1: View All VMAs of a Process

```bash
cat /proc/self/maps

# WHAT: Each line = one VMA
# WHY: Kernel exposes process memory layout
# WHERE: Maple tree traversal in seq_file read
# WHO: Kernel fills, tools read
# WHEN: On read(), kernel iterates VMAs
# WITHOUT: Need kernel debugger to see VMA list
# WHICH: Columns = start-end perms offset dev inode path

# EXAMPLE:
# 00400000-00401000 r--p 00000000 08:01 12345 /bin/cat
# ^^^^^^^^ ^^^^^^^^ ^^^^ ^^^^^^^^ ^^^^^ ^^^^^ ^^^^^^^^^
#    │        │      │      │       │     │      │
#    │        │      │      │       │     │      └─ mapped file
#    │        │      │      │       │     └──────── inode
#    │        │      │      │       └────────────── major:minor
#    │        │      │      └────────────────────── file offset
#    │        │      └───────────────────────────── r=read,w=write,x=exec,p=private
#    │        └──────────────────────────────────── vm_end (exclusive)
#    └───────────────────────────────────────────── vm_start

# CALCULATION for one VMA:
# start = 0x00400000 = 4194304
# end = 0x00401000 = 4198400
# size = 4198400 - 4194304 = 4096 bytes = 1 page
```

### COMMAND 2: Count VMAs per Process

```bash
for pid in $(ps -eo pid --no-headers | head -20); do
    count=$(cat /proc/$pid/maps 2>/dev/null | wc -l)
    echo "PID $pid: $count VMAs"
done | sort -t: -k2 -n | tail -10

# TYPICAL OUTPUT:
# PID 1234: 150 VMAs (browser tab)
# PID 5678: 50 VMAs (simple daemon)
#
# CALCULATION:
# Simple process: 10-30 VMAs
#   text, data, bss, heap, stack, libc, ld.so, vdso, vsyscall
# Complex process (Chrome): 500+ VMAs
#   Each .so = 3-5 VMAs (text, rodata, data, bss)
#   50 shared libs × 4 = 200 VMAs from libs alone
#
# MAPLE TREE BENEFIT:
# RB-tree lookup 500 entries: log2(500) = 9 steps × 100ns = 900ns
# Maple tree lookup: ~4 node reads × 100ns = 400ns
# Speedup: 2.25×
```

### COMMAND 3: Force VMA Split

```bash
cat << 'EOF' > /tmp/vma_split.c
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

int main() {
    // Allocate 3 pages
    void *ptr = mmap(NULL, 3*4096, PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    
    printf("Before mprotect:\n");
    system("grep -A1 'heap\\|anon' /proc/$PPID/maps | head -5");
    
    // Change middle page to read-only → splits VMA!
    mprotect(ptr + 4096, 4096, PROT_READ);
    
    printf("\nAfter mprotect (VMA split!):\n");
    system("grep -A2 'rw-p\\|r--p' /proc/$PPID/maps | tail -6");
    
    munmap(ptr, 3*4096);
}
EOF
gcc /tmp/vma_split.c -o /tmp/vma_split && /tmp/vma_split

# BEFORE:
# 7f0000000000-7f0000003000 rw-p ... (3 pages, 1 VMA)
#
# AFTER mprotect:
# 7f0000000000-7f0000001000 rw-p ... (page 1)
# 7f0000001000-7f0000002000 r--p ... (page 2, protected)
# 7f0000002000-7f0000003000 rw-p ... (page 3)
#
# 1 VMA → 3 VMAs from mprotect!
#
# MEMORY OVERHEAD:
# sizeof(struct vm_area_struct) ≈ 200 bytes
# 3 VMAs instead of 1 = 600 - 200 = 400 bytes extra
```

### COMMAND 4: VMA Merge Demonstration

```bash
cat << 'EOF' > /tmp/vma_merge.c
#include <stdio.h>
#include <sys/mman.h>

int main() {
    // Allocate two adjacent regions
    void *p1 = mmap((void*)0x10000000, 4096, PROT_READ|PROT_WRITE,
                    MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
    void *p2 = mmap((void*)0x10001000, 4096, PROT_READ|PROT_WRITE,
                    MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
    
    printf("Two mmap calls, but check VMAs:\n");
    system("grep 10000 /proc/$PPID/maps");
    
    // Likely shows ONE merged VMA:
    // 10000000-10002000 rw-p ... (merged!)
}
EOF
gcc /tmp/vma_merge.c -o /tmp/vma_merge && /tmp/vma_merge

# PARADOX: Two mmap() calls but ONE VMA?
# ANSWER: Kernel merges adjacent VMAs with same attributes
# VMA count reduction: 2 → 1 = 50% savings
```

---

## FINAL PARADOX QUESTIONS

```
Q1: find_vma(mm, addr) returns VMA AFTER addr if addr not in any VMA?
    
    ANSWER:
    VMAs: [0x1000,0x2000), [0x3000,0x4000)
    find_vma(0x2500):
      0x2500 not in any VMA (hole)
      Returns VMA at [0x3000,0x4000)
      Must check: if (vma && addr >= vma->vm_start) → in VMA
      If addr < vma->vm_start → in hole → segfault on access
    
Q2: Why is vm_end exclusive (first byte AFTER region)?
    
    CALCULATION:
    VMA = [0x1000, 0x2000)
    Inclusive end would be 0x1FFF
    Size = 0x1FFF - 0x1000 + 1 = 0x1000 = 4096 ✓
    Exclusive: size = 0x2000 - 0x1000 = 0x1000 = 4096 ✓
    
    Adjacent VMAs: [0x1000,0x2000), [0x2000,0x3000)
    Inclusive: end of first (0x1FFF) + 1 = start of second? 0x2000 ✓
    Exclusive: end of first (0x2000) = start of second (0x2000) ✓
    
    Exclusive is simpler arithmetic!
    
Q3: mprotect on 1 byte protects entire page (4KB)?
    
    ANSWER:
    Protection granularity = page size = 4KB
    mprotect(ptr, 1, PROT_READ):
      start_page = ptr & ~0xFFF
      end_page = (ptr + 1 + 0xFFF) & ~0xFFF
      Protects entire page containing that 1 byte
    VMA split still happens at PAGE BOUNDARY
```
