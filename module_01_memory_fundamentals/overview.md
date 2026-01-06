# Module 1: Linux Memory Fundamentals

## Overview

This module covers the foundational concepts of Linux memory management. You will learn how the CPU translates virtual addresses to physical addresses using page tables, and how the kernel optimizes this process.

## Learning Objectives

By the end of this module, you will be able to:

1. Explain the 4-level page table structure on x86_64
2. Describe the role of CR3 register in address translation
3. Understand TLB operation and PCID optimization
4. Trace a page walk from virtual to physical address
5. Define VMA and its role in process memory management

## Key Concepts

### Page Tables

The CPU uses a 4-level page table hierarchy to translate 48-bit virtual addresses to physical addresses:

```
Virtual Address (48 bits):
┌─────────┬─────────┬─────────┬─────────┬──────────────┐
│ PGD (9) │ PUD (9) │ PMD (9) │ PTE (9) │ Offset (12)  │
└─────────┴─────────┴─────────┴─────────┴──────────────┘
     ↓         ↓         ↓         ↓          ↓
   Level 4   Level 3   Level 2   Level 1   Page offset
```

Each level contains 512 entries (2^9), each pointing to the next level or the final physical page.

### CR3 Register

CR3 holds the physical address of the top-level page table (PGD). When the OS switches processes, it loads the new process's page table address into CR3.

### TLB (Translation Lookaside Buffer)

The TLB caches recent virtual-to-physical translations. Without TLB, every memory access would require 4 additional memory reads for the page walk.

### VMA (Virtual Memory Area)

VMAs represent contiguous regions of virtual memory with uniform permissions. The kernel tracks VMAs using a maple tree for O(log n) lookups.

## Hands-On Files

| File | Description |
|------|-------------|
| `page-walks.md` | Step-by-step page walk derivation |
| `cr3-register.md` | CR3 mechanics and process switching |
| `tlb-pcid.md` | TLB operation and PCID optimization |
| `vma.md` | VMA structure and lookup |

## Prerequisites

- Understanding of binary and hexadecimal numbers
- Basic knowledge of CPU architecture
- Familiarity with Linux process concept

## Next Module

[Module 2: Page Fault Handling →](../module_02_page_fault/)
