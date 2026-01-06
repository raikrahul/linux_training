# Module 9: Maple Tree & VMA

## Overview

This module explores the maple tree data structure used by Linux to manage Virtual Memory Areas (VMAs). You will understand how the kernel achieves O(log n) VMA lookups.

## Learning Objectives

By the end of this module, you will be able to:

1. Explain VMA organization in process address space
2. Describe the maple tree structure
3. Trace a VMA lookup from virtual address
4. Understand maple tree advantages over red-black trees
5. Analyze VMA flags and permissions

## Key Concepts

### VMA (Virtual Memory Area)

A VMA represents a contiguous region of virtual memory:

```c
struct vm_area_struct {
    unsigned long vm_start;     // Start address
    unsigned long vm_end;       // End address (exclusive)
    struct file *vm_file;       // Backing file (or NULL)
    unsigned long vm_flags;     // Permissions and flags
    // ...
};
```

### Process Address Space

```
┌────────────────────────┐ 0xFFFF...
│     Kernel Space       │
├────────────────────────┤
│     Stack (VMA)        │
├────────────────────────┤
│         ...            │
├────────────────────────┤
│     Heap (VMA)         │
├────────────────────────┤
│   Data/BSS (VMA)       │
├────────────────────────┤
│     Text (VMA)         │
└────────────────────────┘ 0x0
```

### Maple Tree

The maple tree replaced red-black trees for VMA management in Linux 6.1:

Advantages:
- RCU-safe iteration
- Better cache locality
- Simpler locking
- Range-based operations

```c
// VMA lookup
struct vm_area_struct *vma = find_vma(mm, address);
// Uses maple tree internally
```

### VMA Flags

| Flag | Meaning |
|------|---------|
| VM_READ | Readable |
| VM_WRITE | Writable |
| VM_EXEC | Executable |
| VM_SHARED | Shared mapping |
| VM_GROWSDOWN | Stack (grows down) |

## Hands-On Files

| File | Description |
|------|-------------|
| `vma_lookup_worksheet.md` | VMA lookup tracing |

## Prerequisites

- Module 1: Memory Fundamentals
- Module 2: Page Fault Handling
- Tree data structure basics

## Next Module

[Module 10: NUMA & Zones →](../module_10_numa_zones/)
