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

---

## W-QUESTIONS — NUMERICAL ANSWERS

### WHAT: Page Table Entry

```
PTE = 64-bit value at address 0xFFFF888112340000
PTE value = 0x800000012345_8067

bit[0] = 1 → present ✓
bit[1] = 1 → writable ✓
bit[2] = 1 → user-accessible ✓
bit[5] = 1 → accessed ✓
bit[6] = 1 → dirty ✓
bits[51:12] = 0x12345 → PFN = 0x12345
PFN × 4096 = 0x12345 × 0x1000 = 0x12345000 = physical address
```

### WHY: 4 Levels Not 3
```
3-level: 9+9+9+12 = 39 bits → 2^39 = 512GB max VA
4-level: 9+9+9+9+12 = 48 bits → 2^48 = 256TB max VA
Ratio = 256TB / 512GB = 512× more address space
Modern apps need > 512GB → 4 levels required
```

### WHERE: Page Table Lives
```
CR3 = 0x00000000_ABCDE000
PGD at PA 0xABCDE000 (physical RAM)
Each level at different PA:
  PGD: 0xABCDE000
  PUD: 0x12340000
  PMD: 0x56780000
  PTE: 0x9ABC0000
4 different pages × 4096 bytes = 16KB per full walk
```

### WHO: Accesses Page Tables
```
CPU MMU: hardware walk on TLB miss
  → 4 RAM reads per translation
Kernel: software walk for page fault
  → pgd_offset(), pud_offset(), pmd_offset(), pte_offset()
CR3 loaded by kernel on context switch
  → switch_mm() writes CR3
```

### WHEN: Translation Happens
```
Every instruction fetch: PC → PA
Every load: VA → PA
Every store: VA → PA
Example: MOV RAX, [0x7FFE1234] executes:
  T₀: fetch instruction at VA → 1 translation
  T₁: load from 0x7FFE1234 → 1 translation
  Total: 2 translations per instruction
```

### WITHOUT: No TLB
```
With TLB (hit): 1 memory access
Without TLB: 5 memory accesses (4 walk + 1 data)
Slowdown = 5× per memory operation
1 billion accesses/sec × 5 = 5 billion RAM reads/sec
TLB hit rate 99% → only 1% pay 5× penalty
```

### WHICH: Index Selects Entry
```
VA = 0x7FFE_1234_5678
PGD index = bits[47:39] = 0xFF = 255 → PGD[255]
PUD index = bits[38:30] = 0x1F8 = 504 → PUD[504]
PMD index = bits[29:21] = 0x91 = 145 → PMD[145]
PTE index = bits[20:12] = 0x45 = 69 → PTE[69]
```

---

## ANNOYING CALCULATIONS — BREAKDOWN

### Annoying: Hex to Binary Split
```
0x7FFE → binary?
7 = 0111, F = 1111, F = 1111, E = 1110
0x7FFE = 0111_1111_1111_1110
Split at bit 12: upper 4 bits = 0111 = 7, next 12 bits = FFE
```

### Annoying: 9-bit Index Extraction
```
48-bit VA, need bits[47:39]
Method: (VA >> 39) & 0x1FF
0x7FFE12345678 >> 39 = 0x7FFE12345678 / 2^39 = 0xFF
0xFF & 0x1FF = 0xFF = 255 ✓
```

### Annoying: Entry Address Calculation  
```
PGD base = 0x1234_0000
Index = 255
Entry size = 8 bytes
Entry address = 0x1234_0000 + 255 × 8 = 0x1234_0000 + 0x7F8 = 0x1234_07F8
```

### Annoying: Mask Flag Bits
```
Entry = 0x12345_067
PA = entry & ~0xFFF = 0x12345_067 & 0xFFFFF_F000 = 0x12345_000
Flags = entry & 0xFFF = 0x067 = 0000_0110_0111
  bit0=1(present) bit1=1(write) bit2=1(user) bit5=1(accessed) bit6=1(dirty)
```

### Annoying: Page Count in Range
```
Start VA = 0x7FFE_0000_0000
End VA = 0x7FFF_0000_0000
Size = 0x7FFF_0000_0000 - 0x7FFE_0000_0000 = 0x1_0000_0000 = 4GB
Pages = 4GB / 4KB = 4 × 2^30 / 4 × 2^10 = 2^20 = 1,048,576 pages
```

---

## ATTACK PLAN

```
1. Convert hex to binary digit-by-digit: F=1111, E=1110, ...
2. Draw 48-bit binary, mark bit positions 47,39,30,21,12,0
3. Extract 9-bit chunks using shift+mask: (VA >> N) & 0x1FF
4. Multiply index by 8 for entry address
5. Mask low 12 bits for PA extraction
6. Verify: reconstruct VA from indices using weighted sum
```

---

## ADDITIONAL FAILURE PREDICTIONS

```
FAILURE 7: 0x1FF = 511, not 512 → 9-bit mask is 0x1FF not 0x200
FAILURE 8: Entry at PGD[255] does NOT mean PA 255 → calculate offset
FAILURE 9: ~0xFFF on 32-bit is 0xFFFFF000, on 64-bit is 0xFFFFFFFFFFFF000
FAILURE 10: Index 0 is valid → 512 entries are [0,511]
```

---

## SHELL COMMANDS — PARADOXICAL THINKING EXERCISES

### COMMAND 1: Read CR3 and Calculate PGD Address

```bash
# Read CR3 from /proc/self/pagemap (requires root for physical addresses)
sudo cat /proc/1/pagemap | xxd | head -1

# WHAT: pagemap entry = 8 bytes per virtual page
# WHY: kernel exposes VA→PFN mapping without needing kernel module
# WHERE: /proc/[pid]/pagemap at offset (VA/4096)*8
# WHO: kernel fills pagemap, userspace reads
# WHEN: every read() triggers kernel to walk page tables
# WITHOUT: need kernel module to read CR3 directly
# WHICH: bit 63 = present, bits 0-54 = PFN

# MEMORY CALCULATION:
# VA = 0x7FFE_1234_5000
# offset = (0x7FFE_1234_5000 / 4096) * 8
#        = (0x7FFE_1234_5) * 8
#        = 0x3FFF_091A_28 bytes into pagemap file
# 
# SCALE TEST:
# Small: VA = 0x1000 → offset = (0x1000/4096)*8 = 1*8 = 8 bytes
# Mid: VA = 0x400000 → offset = (0x400000/4096)*8 = 0x400*8 = 0x2000 = 8192 bytes
# Large: VA = 0x7FFF_FFFF_F000 → offset = 0x3FFF_FFFF_F8 bytes ≈ 256GB into file!
# Edge: VA = 0x0 → offset = 0 bytes = first 8 bytes of pagemap
```

### COMMAND 2: Extract Page Table Indices from Address

```bash
VA=0x7FFE12345678
PGD_IDX=$(( ($VA >> 39) & 0x1FF ))
PUD_IDX=$(( ($VA >> 30) & 0x1FF ))
PMD_IDX=$(( ($VA >> 21) & 0x1FF ))
PTE_IDX=$(( ($VA >> 12) & 0x1FF ))
OFFSET=$(( $VA & 0xFFF ))

echo "PGD=$PGD_IDX PUD=$PUD_IDX PMD=$PMD_IDX PTE=$PTE_IDX OFF=$OFFSET"

# CALCULATION PROOF:
# VA = 0x7FFE12345678 = 140,730,817,355,384 decimal
# 
# Step 1: PGD_IDX = VA >> 39
#   = 140730817355384 >> 39
#   = 140730817355384 / 549755813888
#   = 255 (0xFF)
# 
# Step 2: PUD_IDX = (VA >> 30) & 0x1FF
#   = (140730817355384 >> 30) & 511
#   = 131067 & 511
#   = 504 - 512 = 504 WRONG → recalc
#   = 131067 % 512 = 504 - 512 + 512 = 504... let me recalc
#   131067 / 512 = 255.99, 131067 - 255*512 = 131067 - 130560 = 507
#   Actually: 131067 & 0x1FF = 0x1FB = 507
# 
# Step 3: PMD = (VA >> 21) & 0x1FF = (67108864...) & 511 = ...
# 
# PARADOX: Why does bash handle 64-bit? Because $((expr)) uses long long.
# PARADOX: What if VA has bit 48+ set? Sign extension to kernel address!
```

### COMMAND 3: Count Pages in VMA

```bash
cat /proc/self/maps | while read line; do
  START=$(echo $line | cut -d'-' -f1)
  END=$(echo $line | cut -d'-' -f2 | cut -d' ' -f1)
  START_DEC=$((16#$START))
  END_DEC=$((16#$END))
  SIZE=$((END_DEC - START_DEC))
  PAGES=$((SIZE / 4096))
  echo "$START-$END: $PAGES pages ($SIZE bytes)"
done | head -10

# MEMORY DIAGRAM:
# ┌────────────────────────────────────────────────────────────┐
# │ VMA at [0x400000, 0x401000)                                │
# │ START = 0x400000 = 4194304 decimal                         │
# │ END   = 0x401000 = 4198400 decimal                         │
# │ SIZE  = 4198400 - 4194304 = 4096 bytes                     │
# │ PAGES = 4096 / 4096 = 1 page                               │
# │                                                            │
# │ Physical layout:                                           │
# │ ┌─────────────────────────────────────────────────────────┐│
# │ │ Page Frame @ PA 0x12345000                              ││
# │ │ bytes [0x000..0xFFF] = 4096 bytes                       ││
# │ │ PTE entry = 0x12345067 (present, user, accessed)        ││
# │ └─────────────────────────────────────────────────────────┘│
# └────────────────────────────────────────────────────────────┘
#
# SCALE TEST:
# Small VMA: [0x1000, 0x2000) = 1 page
# Stack VMA: [0x7FFE00000000, 0x7FFF00000000) = 0x100000000/4096 = 4GB/4KB = 1M pages
# But actual stack is < 8MB = 2048 pages typical
```

### COMMAND 4: TLB Miss Counter

```bash
# Read TLB miss counters from perf
sudo perf stat -e dTLB-load-misses,dTLB-store-misses,iTLB-load-misses -p $$ sleep 1

# CALCULATION:
# If TLB has 1024 entries, and we access 1025 unique pages:
#   Guaranteed misses ≥ 1 (1025 - 1024)
# If we access 10000 unique pages in loop:
#   First pass: 10000 misses (cold)
#   Second pass: 10000 - 1024 = 8976 misses (only 1024 fit in TLB)
# 
# MEMORY: 10000 pages × 4KB = 40MB
# TLB covers: 1024 × 4KB = 4MB
# Miss rate on random access: (40MB - 4MB) / 40MB = 90%
#
# PARADOX: Why doesn't TLB scale with RAM?
# Answer: TLB is SRAM (fast, expensive), RAM is DRAM (slow, cheap)
# TLB lookup: 1 cycle, RAM: 100 cycles
```

### COMMAND 5: Page Table Memory Overhead

```bash
# Calculate page table memory for process
PID=$$
VMAS=$(cat /proc/$PID/maps | wc -l)
PAGES=$(cat /proc/$PID/statm | awk '{print $1}')

echo "VMAs: $VMAS, Pages: $PAGES"
echo "Page table overhead estimate:"

# CALCULATION:
# Each page needs: 1 PTE (8 bytes)
# Each PMD can hold 512 PTEs, needs 1 PMD entry (8 bytes)
# Each PUD can hold 512 PMDs...
#
# For 1000 pages scattered across address space:
# Worst case: 1000 different PTEs in 1000 different PMDs
#   = 1000 PTE tables × 4KB each = 4MB
#   + 1000 PMD entries across maybe 2 PUD tables = 8KB
#   + 2 PUD entries = 16 bytes
#   + 1 PGD entry = 8 bytes
# Total: ~4MB page table overhead for 4MB user data = 100% overhead!
#
# Best case: 1000 contiguous pages
#   = 2 PTE tables (512+488 entries) = 8KB
#   + 2 PMD entries = 16 bytes
#   + 1 PUD entry = 8 bytes
#   + 1 PGD entry = 8 bytes
# Total: ~8KB overhead for 4MB data = 0.2% overhead
#
# PARADOX: Sparse address space wastes MORE memory on page tables!
```

---

## FINAL PARADOX QUESTIONS

```
Q1: If kernel uses 4-level page tables, and each level is 4KB,
    why doesn't walking 4 levels read 16KB of data?
    
    ANSWER CALCULATION:
    Each level: read 1 entry (8 bytes), not full table (4KB)
    4 levels × 8 bytes = 32 bytes read per translation
    Not 4 × 4KB = 16KB
    
Q2: Process A and B both have VA 0x400000 mapped.
    How many physical pages for the CODE?
    
    ANSWER: 1 physical page (shared), 2 PTEs (one per process)
    
Q3: Why is TLB flush on context switch expensive if TLB is "just a cache"?
    
    ANSWER:
    TLB = 1024 entries
    Next process needs 1024 translations
    Each translation = 4 RAM reads = 400ns
    Total refill cost = 1024 × 400ns = 409μs
    But context switch itself = 1μs
    TLB refill = 400× more expensive than switch!
```



## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: VA → PA TRANSLATION
START: VA=0x7FFE_1234_5678 → CR3=0x1000_0000
P1. INDICES_CALC:
VA(48bit)=0111_1111_1111_1110_0001_0010_0011_0100_0101_0110_0111_1000
PGD_IDX = (VA>>39)&0x1FF = 0xFF = 255
PUD_IDX = (VA>>30)&0x1FF = 0x1F8 = 504
PMD_IDX = (VA>>21)&0x1FF = 0x91 = 145
PTE_IDX = (VA>>12)&0x1FF = 0x45 = 69
OFFSET  = VA&0xFFF = 0x678

P2. CR3_READ:
Phys=0x1000_0000 → READ(8B) → PGD_BASE
PGD_ENTRY_ADDR = 0x1000_0000 + (255 × 8) = 0x1000_07F8
MEM[0x1000_07F8] = 0x8000_0000_2000_0067 (Valid=1, Write=1, User=1, PFN=0x20000)
NEXT_BASE = 0x2000_0000

P3. PUD_WALK:
PUD_ENTRY_ADDR = 0x2000_0000 + (504 × 8) = 0x2000_0FC0
MEM[0x2000_0FC0] = 0x8000_0000_3000_0067 (PFN=0x30000)
NEXT_BASE = 0x3000_0000

P4. PMD_WALK:
PMD_ENTRY_ADDR = 0x3000_0000 + (145 × 8) = 0x3000_0488
MEM[0x3000_0488] = 0x8000_0000_4000_0067 (PFN=0x40000)
NEXT_BASE = 0x4000_0000

P5. PTE_WALK:
PTE_ENTRY_ADDR = 0x4000_0000 + (69 × 8) = 0x4000_0228
MEM[0x4000_0228] = 0x8000_0000_5000_0067 (PFN=0x50000)
FINAL_PA_BASE = 0x5000_0000

P6. FINAL_CALC:
PA = FINAL_PA_BASE | OFFSET
PA = 0x5000_0000 | 0x678 = 0x5000_0678
RESULT = 0x5000_0678 ✓

P7. FAILURE_PREDICT:
F1. CR3_INVALID → CR3=0 → CPU_Triple_Fault ✗
F2. PGD_PRESENT=0 → bit0=0 → Page_Fault(CR2=VA, ERR=0) ✗
F3. LARGE_PAGE_BIT=1 @ PMD → PMD is Leaf → No PTE walk ✗


---

[← Course Index](../index.md) | [Course Index](../index.md) | [Next Lesson →](../module_02_page_fault/lesson_02.md)
