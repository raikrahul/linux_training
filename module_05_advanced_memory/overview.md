# Module 5: Advanced Memory Topics

## Overview

This module covers advanced kernel memory management topics including LRU lists, page cache, anonymous memory, and mlock.

---

## 1. LRU Lists

### Purpose

The kernel must decide which pages to evict when memory is low. LRU (Least Recently Used) lists track page access patterns.

### List Organization

```
Per-Node LRU Lists:
┌──────────────────────────────────────────────────────────┐
│                                                          │
│  Active Anonymous    ◄────────►  Inactive Anonymous      │
│  (recently used)                 (candidates for swap)   │
│                                                          │
│  Active File         ◄────────►  Inactive File           │
│  (recently used)                 (candidates for drop)   │
│                                                          │
│  Unevictable                                             │
│  (mlock'd pages)                                         │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### Page Flow

```
New anonymous page:
    alloc_page() → add to Inactive Anonymous
         │
         ▼
    Page accessed (PG_referenced set)
         │
         ▼
    kswapd scans, sees referenced
         │
         ▼
    Promote to Active Anonymous
         │
         ▼
    No access for a while
         │
         ▼
    Demote back to Inactive
         │
         ▼
    Still no access, swapped out
```

### Code: Adding to LRU

```c
// mm/swap.c
void lru_cache_add(struct page *page)
{
    struct lruvec *lruvec;
    
    // Determine which list
    if (PageSwapBacked(page))
        lru = LRU_INACTIVE_ANON;  // Anonymous
    else
        lru = LRU_INACTIVE_FILE;  // File-backed
    
    // Add to list
    list_add(&page->lru, &lruvec->lists[lru]);
    
    // Mark page as on LRU
    SetPageLRU(page);
}
```

---

## 2. Page Cache

### What is Page Cache?

The page cache stores file data in RAM:

```
Application calls read(fd, buf, 4096):

Step 1: Check page cache
        ┌─────────────────────────────────────────┐
        │  address_space for this file            │
        │  ┌─────────────────────────────────────┐│
        │  │ Radix tree / XArray                 ││
        │  │                                     ││
        │  │ Index 0 → struct page (data)        ││
        │  │ Index 1 → struct page (data)        ││
        │  │ Index 2 → NULL (not cached)         ││
        │  │ Index 3 → struct page (data)        ││
        │  └─────────────────────────────────────┘│
        └─────────────────────────────────────────┘

Step 2: Cache hit  → copy from page to user buffer
        Cache miss → read from disk, add to cache, copy
```

### Code Flow

```c
// fs/read_write.c (simplified)
ssize_t vfs_read(struct file *file, char __user *buf, size_t count)
{
    // ...
    return file->f_op->read_iter(kio, iter);
}

// mm/filemap.c
ssize_t generic_file_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
    struct address_space *mapping = file->f_mapping;
    pgoff_t index = pos >> PAGE_SHIFT;
    
    // Try to find page in cache
    page = find_get_page(mapping, index);
    
    if (!page) {
        // Cache miss - read from disk
        page = page_cache_alloc(mapping);
        error = mapping->a_ops->read_folio(file, folio);
        add_to_page_cache(page, mapping, index);
    }
    
    // Copy to user buffer
    copy_page_to_iter(page, offset, bytes, iter);
}
```

### Viewing Page Cache

```bash
$ free -h
              total        used        free      shared  buff/cache   available
Mem:           15Gi       4.2Gi       8.1Gi       256Mi       3.2Gi        10Gi

# buff/cache = 3.2GB is page cache + buffers

$ cat /proc/meminfo | grep -E "Cached|Buffers"
Buffers:          234568 kB
Cached:          3145728 kB
```

---

## 3. Anonymous Memory

### What is Anonymous Memory?

Anonymous pages have no file backing:
- Stack allocations
- Heap (malloc)
- MAP_ANONYMOUS mmap

```c
// Anonymous mmap
void *p = mmap(NULL, 4096, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
// Creates anonymous VMA
// First access triggers anonymous page fault
```

### Anonymous Page Allocation

```c
// mm/memory.c
static vm_fault_t do_anonymous_page(struct vm_fault *vmf)
{
    struct page *page;
    
    // Allocate zeroed page
    page = alloc_zeroed_user_highpage_movable(vma, vmf->address);
    
    // Set up page structure
    __SetPageSwapBacked(page);  // Can be swapped
    page->mapping = (void *)vma->anon_vma | PAGE_MAPPING_ANON;
    
    // Install in page table
    entry = mk_pte(page, vma->vm_page_prot);
    set_pte_at(mm, vmf->address, vmf->pte, entry);
    
    // Add to LRU
    lru_cache_add_inactive_or_unevictable(page, vma);
}
```

---

## 4. mlock - Pinning Pages

### Why mlock?

Prevent pages from being swapped out:
- Cryptographic keys (security)
- Real-time applications (deterministic latency)
- Database buffers (performance)

### User API

```c
#include <sys/mman.h>

int main() {
    void *ptr = malloc(4096 * 100);  // 400KB
    
    // Lock in RAM
    if (mlock(ptr, 4096 * 100) < 0) {
        perror("mlock");
        return 1;
    }
    
    // ... use memory (guaranteed no page faults) ...
    
    // Unlock
    munlock(ptr, 4096 * 100);
}
```

### Kernel Implementation

```c
// mm/mlock.c
static int mlock_fixup(struct vm_area_struct *vma, unsigned long start,
                       unsigned long end, vm_flags_t newflags)
{
    // Set VM_LOCKED flag on VMA
    vma->vm_flags = newflags;
    
    // Populate the pages now
    if (newflags & VM_LOCKED) {
        // Fault in all pages
        populate_vma_page_range(vma, start, end);
        
        // Move pages to unevictable LRU
        mlock_vma_pages_range(vma, start, end);
    }
}
```

### LRU Effect

```
Before mlock:
  Page on LRU_INACTIVE_ANON
  ┌─────────────────────┐
  │ page->flags         │ PG_lru set
  │ page->lru           │ linked in lruvec
  └─────────────────────┘

After mlock:
  Page on LRU_UNEVICTABLE
  ┌─────────────────────┐
  │ page->flags         │ PG_mlocked set, PG_unevictable set
  │ page->lru           │ linked in unevictable list
  └─────────────────────┘
```

---

## 5. Kernel Module Example

```c
// lru_trace.c
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/swap.h>

static void print_lru_stats(void)
{
    pg_data_t *pgdat;
    struct lruvec *lruvec;
    int nid;
    
    for_each_online_node(nid) {
        pgdat = NODE_DATA(nid);
        lruvec = &pgdat->__lruvec;
        
        pr_info("Node %d LRU stats:\n", nid);
        pr_info("  Active Anon:    %lu pages\n",
                lruvec_page_state(lruvec, NR_ACTIVE_ANON));
        pr_info("  Inactive Anon:  %lu pages\n",
                lruvec_page_state(lruvec, NR_INACTIVE_ANON));
        pr_info("  Active File:    %lu pages\n",
                lruvec_page_state(lruvec, NR_ACTIVE_FILE));
        pr_info("  Inactive File:  %lu pages\n",
                lruvec_page_state(lruvec, NR_INACTIVE_FILE));
        pr_info("  Unevictable:    %lu pages\n",
                lruvec_page_state(lruvec, NR_MLOCK));
    }
}

static int __init lru_trace_init(void)
{
    print_lru_stats();
    return 0;
}
module_init(lru_trace_init);

static void __exit lru_trace_exit(void) {}
module_exit(lru_trace_exit);
MODULE_LICENSE("GPL");
```

---

## 6. Practice Exercises

### Exercise 1: Page Cache Analysis

Write a program that:
1. Opens a large file
2. Reads it sequentially
3. Checks /proc/meminfo Cached before and after
4. Calculates cache hit rate

### Exercise 2: mlock Impact

Create a program that:
1. Allocates 100MB
2. mlocks it
3. Checks /proc/self/status for VmLck
4. Measures page fault time with/without mlock

### Exercise 3: LRU Visualization

Write a kernel module that:
1. Samples LRU list sizes every second
2. Tracks promotion/demotion events
3. Outputs to dmesg

---

## Next Module

[Module 6: Kprobe Tracing →](../module_06_kprobe_tracing/)

[← Back to Course Index](../index.md)

---

## AXIOMATIC EXERCISES — BRUTE FORCE CALCULATION

### EXERCISE A: LRU LIST TRANSITIONS

```
GIVEN: New anonymous page allocated

TASK: Track lru field and flags through lifecycle

T1: alloc_page() called
    page->lru = disconnected
    PG_lru = ___, PG_active = ___

T2: lru_cache_add_inactive_or_unevictable()
    page added to: LRU_INACTIVE_ANON / LRU_ACTIVE_ANON?
    PG_lru = ___, PG_active = ___

T3: Page accessed, mark_page_accessed() called
    PG_referenced = ___

T4: kswapd scans, sees PG_referenced set
    Action: promote to active → PG_active = ___
    Clear PG_referenced = ___

T5: No access for long time, demote
    Move to: LRU_INACTIVE_ANON
    PG_active = ___
```

### EXERCISE B: PAGE CACHE HIT CALCULATION

```
GIVEN:
  File size = 10MB = 10 × 1024 × 1024 bytes
  Page size = 4096 bytes
  Cache has pages for offsets: 0, 4096, 8192, 12288, 16384

TASK:

1. Total pages in file = ceil(10MB / 4096) = ceil(___) = ___ pages
2. Cached pages = ___
3. Cache coverage = ___ / ___ = ___% 
4. Read at offset 5000:
   - Page offset = floor(5000 / 4096) = ___
   - In cache? page ___ → YES/NO
5. Read at offset 20000:
   - Page offset = floor(20000 / 4096) = ___
   - In cache? page ___ → YES/NO → cache MISS
```

### EXERCISE C: MLOCK CALCULATION

```
GIVEN:
  mlock(ptr, 100000) called
  ptr = 0x7F0000001234

TASK:

1. Start page = floor(0x7F0000001234 / 4096) = ___
2. End address = 0x7F0000001234 + 100000 = 0x___
3. End page = ceil(0x___ / 4096) = ___
4. Pages to lock = end_page - start_page = ___
5. Memory locked = ___ × 4096 = ___ bytes

TRICKY: mlock locks entire pages containing the range
```

### EXERCISE D: INACTIVE LIST RATIO

```
GIVEN zone stats:
  NR_ACTIVE_ANON = 50000 pages
  NR_INACTIVE_ANON = 150000 pages
  NR_ACTIVE_FILE = 30000 pages
  NR_INACTIVE_FILE = 70000 pages

TASK:

1. Anon ratio = active / (active + inactive) = ___ / ___ = ___
2. File ratio = active / (active + inactive) = ___ / ___ = ___
3. Total anon pages = ___ = ___ MB (at 4KB/page)
4. Total file pages = ___ = ___ MB
5. If kswapd reclaims 10000 pages from inactive file:
   New NR_INACTIVE_FILE = ___
   New file ratio = ___
```

---

## FAILURE PREDICTIONS

```
FAILURE 1: Confusing PG_referenced with PG_active → wrong promotion logic
FAILURE 2: Page cache index = offset / 4096, not offset
FAILURE 3: mlock locks PAGES not bytes → boundary alignment matters
FAILURE 4: _mapcount=-1 means not mapped, not refcount=0
FAILURE 5: Inactive list is larger than active → more reclaim candidates
```
