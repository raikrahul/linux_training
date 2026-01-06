# Module 5: Advanced Memory Topics

## Overview

This module covers advanced kernel memory management topics including LRU lists, page cache, anonymous memory, and mlock. You will understand how the kernel decides which pages to keep in memory.

## Learning Objectives

By the end of this module, you will be able to:

1. Explain the LRU algorithm for page reclaim
2. Distinguish active and inactive LRU lists
3. Understand anonymous vs file-backed pages
4. Use mlock to pin pages in memory
5. Trace page cache behavior

## Key Concepts

### LRU Lists

The kernel maintains per-node LRU lists for page reclaim:

```
Active Anonymous    ←→    Inactive Anonymous
Active File         ←→    Inactive File
          ↓                      ↓
    Recently used          Candidates for reclaim
```

Pages move between lists based on access patterns. The `kswapd` daemon scans inactive lists when memory is low.

### Page Cache

The page cache stores file data in memory:

```
read(fd, buf, 4096)
        ↓
Check page cache (address_space)
        ↓
   Cache hit? → copy to user buffer
   Cache miss? → read from disk, add to cache
```

### Anonymous Memory

Anonymous pages have no backing file:
- Stack and heap allocations
- mmap(MAP_ANONYMOUS)
- Copy-on-write pages after fork

### mlock

mlock() prevents pages from being swapped:

```c
void *p = malloc(4096);
mlock(p, 4096);  // Page will stay in RAM
```

Use cases:
- Cryptographic keys (security)
- Real-time applications (latency)
- Database buffers (performance)

## Hands-On Files

| File | Description |
|------|-------------|
| `derivation.md` | LRU algorithm derivation |
| `homework_worksheet.md` | Practice problems |
| `listing_2_3_worksheet.md` | Code analysis |
| `README.md` | Quick reference |

## Prerequisites

- Module 4: struct page
- Understanding of caching concepts
- Process memory model

## Next Module

[Module 6: Kprobe Tracing →](../module_06_kprobe_tracing/)
