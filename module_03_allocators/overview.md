# Module 3: Memory Allocators

## Overview

This module covers the Linux kernel memory allocation subsystem. You will understand how the buddy allocator manages physical pages and how the slab allocator provides efficient object caching.

## Learning Objectives

By the end of this module, you will be able to:

1. Explain the buddy allocator algorithm and free list organization
2. Calculate order from allocation size
3. Understand external fragmentation and buddy splitting
4. Describe slab/slub allocator purpose and operation
5. Use GFP flags correctly in different contexts

## Key Concepts

### Buddy Allocator

The buddy allocator manages physical pages in power-of-2 blocks:

```
Order 0: 4KB pages (2^0 = 1 page)
Order 1: 8KB blocks (2^1 = 2 pages)
Order 2: 16KB blocks (2^2 = 4 pages)
...
Order 10: 4MB blocks (2^10 = 1024 pages)
```

Free blocks are stored in per-order free lists. When allocating:
1. Find smallest order >= requested size
2. If not available, split higher-order block
3. Return one half, add other to lower free list

### Slab Allocator

The slab allocator sits on top of the buddy allocator:

```
Buddy Allocator (pages)
        ↓
Slab Allocator (objects)
        ↓
kmalloc(), kmem_cache_alloc()
```

Benefits:
- Reduces internal fragmentation
- Caches frequently allocated objects
- Maintains constructor/destructor support

### GFP Flags

GFP (Get Free Pages) flags control allocation behavior:

| Flag | Meaning |
|------|---------|
| GFP_KERNEL | Can sleep, can swap, most permissive |
| GFP_ATOMIC | Cannot sleep (interrupt context) |
| GFP_NOWAIT | Don't wait, return NULL if unavailable |
| GFP_DMA | Allocate from DMA-capable zone |

## Hands-On Files

| File | Description |
|------|-------------|
| `buddy-allocator.md` | Buddy algorithm derivation |
| `slab-allocator.md` | Slab internals |
| `derivation.md` | Mathematical foundations |
| `README.md` | Quick reference |

## Prerequisites

- Module 1: Memory Fundamentals
- Understanding of power-of-2 math
- Interrupt context awareness

## Next Module

[Module 4: struct page Deep Dive →](../module_04_struct_page/)
