# Module 1: Linux Memory Fundamentals

## Overview

This module covers the foundational concepts of Linux memory management on x86_64. You will learn how the CPU translates virtual addresses to physical addresses using page tables, and how the kernel optimizes this process.

---

## 1. Virtual vs Physical Addresses

### The Problem

Every process sees its own address space starting at 0x0. Two processes can both use address 0x400000 for their code. How does the CPU know where in RAM each process's data actually lives?

### The Solution: Address Translation

```
PROCESS A uses VA 0x400000 → CPU translates → PA 0x7F000000
PROCESS B uses VA 0x400000 → CPU translates → PA 0x82000000
```

The CPU uses page tables to perform this translation.

---

## 2. Page Tables: 4-Level Structure

On x86_64, the CPU uses 4 levels of page tables:

```
48-bit Virtual Address Layout:
┌─────────┬─────────┬─────────┬─────────┬──────────────┐
│ Bits    │ Bits    │ Bits    │ Bits    │ Bits         │
│ 47-39   │ 38-30   │ 29-21   │ 20-12   │ 11-0         │
│ (9 bits)│ (9 bits)│ (9 bits)│ (9 bits)│ (12 bits)    │
├─────────┼─────────┼─────────┼─────────┼──────────────┤
│ PGD idx │ PUD idx │ PMD idx │ PTE idx │ Page offset  │
└─────────┴─────────┴─────────┴─────────┴──────────────┘
     │         │         │         │          │
     ▼         ▼         ▼         ▼          ▼
  Level 4   Level 3   Level 2   Level 1   Offset in page
```

### Example: Translating VA 0x7FFE12345678

```
VA = 0x7FFE12345678
Binary = 0111 1111 1111 1110 0001 0010 0011 0100 0101 0110 0111 1000

Extract indices:
PGD index = bits[47:39] = 0x0FF = 255
PUD index = bits[38:30] = 0x1F8 = 504
PMD index = bits[29:21] = 0x091 = 145
PTE index = bits[20:12] = 0x145 = 325
Offset    = bits[11:0]  = 0x678 = 1656
```

### Kernel Source: arch/x86/include/asm/pgtable_64_types.h

```c
#define PGDIR_SHIFT     39
#define PUD_SHIFT       30
#define PMD_SHIFT       21
#define PAGE_SHIFT      12

#define PTRS_PER_PGD    512
#define PTRS_PER_PUD    512
#define PTRS_PER_PMD    512
#define PTRS_PER_PTE    512
```

---

## 3. CR3 Register

CR3 holds the physical address of the PGD (top-level page table):

```
CR3 Register (64 bits):
┌────────────────────────────────────────────┬─────────────┐
│ Physical Address of PGD (bits 51:12)       │ Flags (11:0)│
└────────────────────────────────────────────┴─────────────┘
```

### Reading CR3 in Kernel Module

```c
// kernel_module.c
#include <linux/module.h>
#include <asm/processor.h>

static int __init cr3_read_init(void) {
    unsigned long cr3_value;
    
    // Read CR3 using inline assembly
    asm volatile("mov %%cr3, %0" : "=r"(cr3_value));
    
    pr_info("CR3 = 0x%lx\n", cr3_value);
    pr_info("PGD physical address = 0x%lx\n", cr3_value & ~0xFFF);
    
    return 0;
}
module_init(cr3_read_init);
```

### Context Switch and CR3

```
Before switch:
  CR3 = 0x1234000 (Process A's page table)

After switch_mm() called:
  CR3 = 0x5678000 (Process B's page table)
```

Kernel source: arch/x86/mm/tlb.c

```c
void switch_mm(struct mm_struct *prev, struct mm_struct *next,
               struct task_struct *tsk)
{
    // ...
    load_new_mm_cr3(next->pgd, new_asid, true);
    // This writes to CR3
}
```

---

## 4. Page Walk: Step by Step

### Hardware Page Walk Diagram

```
CR3 ──► [PGD Table in RAM]
             │
             │ entry[255]
             ▼
        [PUD Table in RAM]
             │
             │ entry[504]
             ▼
        [PMD Table in RAM]
             │
             │ entry[145]
             ▼
        [PTE Table in RAM]
             │
             │ entry[325]
             ▼
        Physical Page Frame
             │
             │ + offset 1656
             ▼
        Final Physical Address
```

### Manual Page Walk in Userspace

```c
// pagemap_reader.c - Read page tables via /proc/self/pagemap
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

int main() {
    char buffer[4096];
    uint64_t vaddr = (uint64_t)buffer;
    
    // Touch the page to ensure it's mapped
    buffer[0] = 'X';
    
    // Open pagemap
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        perror("open pagemap");
        return 1;
    }
    
    // Calculate offset: each entry is 8 bytes
    uint64_t offset = (vaddr / 4096) * 8;
    lseek(fd, offset, SEEK_SET);
    
    uint64_t entry;
    read(fd, &entry, 8);
    close(fd);
    
    // Parse entry
    uint64_t pfn = entry & ((1ULL << 55) - 1);
    int present = (entry >> 63) & 1;
    
    printf("Virtual Address:  0x%lx\n", vaddr);
    printf("Page Frame Number: 0x%lx\n", pfn);
    printf("Physical Address: 0x%lx\n", pfn * 4096 + (vaddr & 0xFFF));
    printf("Present: %d\n", present);
    
    return 0;
}
```

Output:
```
Virtual Address:  0x7ffd1a234560
Page Frame Number: 0x12a456
Physical Address: 0x12a456560
Present: 1
```

---

## 5. TLB (Translation Lookaside Buffer)

### The Problem: Page Walk is Slow

Every memory access requires 4 additional memory reads:
```
1 read for PGD entry
1 read for PUD entry
1 read for PMD entry
1 read for PTE entry
───────────────────
4 reads per memory access = 4x slower
```

### The Solution: Cache Translations

```
TLB Cache:
┌──────────────────────┬───────────────────┐
│ Virtual Page Number  │ Physical Frame    │
├──────────────────────┼───────────────────┤
│ 0x7FFE12345          │ 0x12A456          │
│ 0x400000             │ 0x7F000           │
│ ...                  │ ...               │
└──────────────────────┴───────────────────┘
```

### TLB Lookup Flow

```
CPU needs to access VA 0x7FFE12345678
         │
         ▼
    ┌───────────┐
    │ Check TLB │
    └─────┬─────┘
          │
    ┌─────┴─────┐
    │           │
  HIT         MISS
    │           │
    ▼           ▼
 Use cached   Do page walk
   PA         Update TLB
```

### PCID (Process Context ID)

Without PCID: TLB flush on every context switch
With PCID: Tag TLB entries with process ID

```
TLB with PCID:
┌──────┬──────────────────────┬───────────────────┐
│ PCID │ Virtual Page Number  │ Physical Frame    │
├──────┼──────────────────────┼───────────────────┤
│  1   │ 0x7FFE12345          │ 0x12A456          │
│  2   │ 0x7FFE12345          │ 0x9ABCD0          │
│ ...  │ ...                  │ ...               │
└──────┴──────────────────────┴───────────────────┘

Same VA, different PA, no flush needed!
```

---

## 6. VMA (Virtual Memory Area)

### What is a VMA?

A VMA represents a contiguous region of virtual memory with uniform attributes:

```c
// include/linux/mm_types.h
struct vm_area_struct {
    unsigned long vm_start;      // First byte of region
    unsigned long vm_end;        // First byte AFTER region
    struct file *vm_file;        // Backing file (or NULL)
    unsigned long vm_flags;      // Permissions and attributes
    pgprot_t vm_page_prot;       // Page protection bits
    // ...
};
```

### Process Memory Layout

```
┌────────────────────────────────┐ 0xFFFFFFFFFFFFFFFF
│                                │
│         Kernel Space           │ (Not accessible to user)
│                                │
├────────────────────────────────┤ 0x7FFFFFFFFFFF
│   [VMA] Stack                  │ ← VM_GROWSDOWN
│         vm_start=0x7FFE00000   │
│         vm_end  =0x7FFF00000   │
│         vm_flags=VM_READ|WRITE │
├────────────────────────────────┤
│                                │
│         (unmapped gap)         │
│                                │
├────────────────────────────────┤
│   [VMA] Heap                   │
│         vm_start=0x01000000    │
│         vm_end  =0x01100000    │
│         vm_flags=VM_READ|WRITE │
├────────────────────────────────┤
│   [VMA] BSS                    │
│         vm_flags=VM_READ|WRITE │
├────────────────────────────────┤
│   [VMA] Data                   │
│         vm_flags=VM_READ|WRITE │
├────────────────────────────────┤
│   [VMA] Text (code)            │
│         vm_start=0x00400000    │
│         vm_flags=VM_READ|EXEC  │
└────────────────────────────────┘ 0x0
```

### Reading VMAs from /proc

```bash
$ cat /proc/self/maps
00400000-00401000 r--p 00000000 08:01 12345  /bin/cat
00401000-00410000 r-xp 00001000 08:01 12345  /bin/cat
00410000-00412000 r--p 00010000 08:01 12345  /bin/cat
7f8a12340000-7f8a12500000 r-xp 00000000 08:01 67890  /lib/libc.so.6
7ffd12340000-7ffd12360000 rw-p 00000000 00:00 0      [stack]
```

Format: `start-end permissions offset device inode pathname`

---

## 7. Practice Exercises

### Exercise 1: Extract Page Table Indices

Given VA = 0x7F1234567890, calculate:
- PGD index
- PUD index  
- PMD index
- PTE index
- Page offset

### Exercise 2: Write a CR3 Reader

Create a kernel module that:
1. Reads CR3 for the current process
2. Walks the page tables manually
3. Verifies a known virtual address maps correctly

### Exercise 3: Analyze /proc/self/maps

Write a C program that:
1. Allocates memory with malloc()
2. mmap()s a file
3. Reads /proc/self/maps
4. Identifies which VMA each allocation belongs to

---

## 8. Key Takeaways

1. Virtual addresses are 48 bits, split into 4 indices + offset
2. CR3 points to the top-level page table (PGD)
3. TLB caches translations to avoid slow page walks
4. PCID allows TLB entries to survive context switches
5. VMAs describe contiguous regions with uniform permissions

---

## Next Module

[Module 2: Page Fault Handling →](../module_02_page_fault/)

[← Back to Course Index](../index.md)

---

## 9. AXIOMATIC EXERCISES — BRUTE FORCE CALCULATION

### EXERCISE A: VA INDEX EXTRACTION

```
GIVEN: VA = 0x7F8A1B2C3D4E
TASK: Extract all indices. DO NOT SKIP STEPS.

1. VA in binary = ?_?_?_?_?_?_?_?_?_?_?_? (fill 48 bits, group by 4)
2. bits[47:39] = ?_?_?_?_?_?_?_?_? (9 bits) → decimal = ___
3. bits[38:30] = ?_?_?_?_?_?_?_?_? (9 bits) → decimal = ___
4. bits[29:21] = ?_?_?_?_?_?_?_?_? (9 bits) → decimal = ___
5. bits[20:12] = ?_?_?_?_?_?_?_?_? (9 bits) → decimal = ___
6. bits[11:0]  = ?_?_?_?_?_?_?_?_?_?_?_? (12 bits) → decimal = ___

VERIFY: PGD_idx × 2^39 + PUD_idx × 2^30 + PMD_idx × 2^21 + PTE_idx × 2^12 + offset = VA ✓ or ✗
```

### EXERCISE B: PAGE TABLE PHYSICAL ADDRESS CHAIN

```
GIVEN:
  CR3 = 0x00000000_12345000
  PGD[255] = 0x00000000_ABCDE003   (bits[11:0] are flags)
  PUD[504] = 0x00000000_98765003
  PMD[145] = 0x00000000_11223003
  PTE[325] = 0x00000000_FFEEDD003

TASK:

1. PGD base PA = CR3 & ~0xFFF = ___________________
2. PGD entry addr = PGD base + (255 × 8) = ___ + ___ = ___________________
3. PUD base PA = PGD[255] & ~0xFFF = ___________________
4. PUD entry addr = PUD base + (504 × 8) = ___ + ___ = ___________________
5. PMD base PA = PUD[504] & ~0xFFF = ___________________
6. PMD entry addr = PMD base + (145 × 8) = ___ + ___ = ___________________
7. PTE base PA = PMD[145] & ~0xFFF = ___________________
8. PTE entry addr = PTE base + (325 × 8) = ___ + ___ = ___________________
9. Page frame PA = PTE[325] & ~0xFFF = ___________________
10. Final PA = Page frame PA + offset(1656) = ___ + ___ = ___________________

TRICKY: entry × 8 because each entry is 8 bytes (64 bits)
TRICKY: & ~0xFFF clears low 12 bits (flags), keeps physical address
```

### EXERCISE C: PAGEMAP CALCULATION

```
GIVEN:
  VA = 0x7FFD_1A23_4560
  Page size = 4096 = 0x1000
  pagemap entry size = 8 bytes

TASK:

1. Page number = VA / 4096 = 0x7FFD_1A23_4560 / 0x1000 = _______________
2. pagemap offset = page_number × 8 = _______________ × 8 = _______________
3. pagemap offset in hex = _______________

USER MUST CALCULATE:
  0x7FFD_1A23_4560 >> 12 = ?
  SHOW DIVISION: 0x7FFD_1A23_4560 = ? × 0x1000 + remainder
```

### EXERCISE D: TLB SIZE CALCULATION

```
GIVEN:
  TLB has 1024 entries
  Each entry: VPN (36 bits) + PFN (40 bits) + flags (8 bits) + PCID (12 bits)

TASK:

1. Entry size = (36 + 40 + 8 + 12) / 8 = ___ bits / 8 = ___ bytes
2. Total TLB size = 1024 × ___ = ___ bytes = ___ KB
3. If page is 4KB, TLB covers ___ × 4KB = ___ MB of virtual memory

VERIFY: 1024 entries × 4KB per page = ___ MB ✓
```

### EXERCISE E: VMA CONTAINS ADDRESS

```
GIVEN VMAs:
┌─────────────────────────────────────────────────────────┐
│ VMA 1: vm_start=0x00400000, vm_end=0x00401000          │
│ VMA 2: vm_start=0x00401000, vm_end=0x00500000          │
│ VMA 3: vm_start=0x7FFE0000, vm_end=0x7FFF0000          │
└─────────────────────────────────────────────────────────┘

TASK: For each address, determine which VMA (or none):

Address 0x00400500: vm_start ≤ addr < vm_end → VMA ___
Address 0x00401000: vm_start ≤ addr < vm_end → VMA ___
Address 0x00500000: vm_start ≤ addr < vm_end → VMA ___
Address 0x7FFEFFFF: vm_start ≤ addr < vm_end → VMA ___
Address 0x7FFF0000: vm_start ≤ addr < vm_end → VMA ___

TRICKY: vm_end is EXCLUSIVE (first byte AFTER region)
```

### EXERCISE F: CONTEXT SWITCH CR3

```
GIVEN:
  Process A: PGD at PA 0x1234_5000
  Process B: PGD at PA 0x5678_9000
  Current CR3 = 0x1234_5000

TASK:

1. CPU running Process A, accesses VA 0x7FFE_0000 → uses CR3 = ___
2. switch_mm() called, loads Process B → CR3 becomes = ___
3. CPU running Process B, accesses VA 0x7FFE_0000 → uses CR3 = ___
4. Same VA, different CR3 → different PA? YES/NO
5. Without PCID, TLB entries from Process A now VALID/INVALID?
```

---

## FAILURE PREDICTIONS

```
FAILURE 1: Forgetting entry size is 8 bytes → wrong pagemap offset
FAILURE 2: Not masking flags with & ~0xFFF → treating flags as address
FAILURE 3: vm_end is exclusive → off-by-one on boundary
FAILURE 4: bits[47:39] means bits 47 down to 39 → 9 bits, not 8
FAILURE 5: Hex to binary conversion error → all indices wrong
FAILURE 6: Forgetting TLB invalidation on CR3 change without PCID
```
