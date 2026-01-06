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

---

## W-QUESTIONS — NUMERICAL ANSWERS

### WHAT: LRU Lists
```
5 LRU lists per node:
  LRU_INACTIVE_ANON: 150000 pages = 585MB
  LRU_ACTIVE_ANON: 50000 pages = 195MB
  LRU_INACTIVE_FILE: 70000 pages = 273MB
  LRU_ACTIVE_FILE: 30000 pages = 117MB
  LRU_UNEVICTABLE: 1000 pages = 3.9MB (mlocked)
Total tracked: 301000 pages = 1.17GB
```

### WHY: Two LRU Lists per Type
```
Single list: scan all 200000 pages to find cold
Active/Inactive split:
  Active: 50000 hot pages, skip all
  Inactive: 150000 cold pages, scan these
Efficiency: scan 150000 instead of 200000 = 25% saved
```

### WHERE: Page Cache Index
```
File: /tmp/data, size = 1MB
Page cache entries:
  index 0 → page at offset 0
  index 1 → page at offset 4096
  index 255 → page at offset 255×4096 = 1044480
Read at byte 100000:
  index = floor(100000 / 4096) = 24
  offset in page = 100000 % 4096 = 1696
```

### WHO: Reclaims Pages
```
kswapd0: kernel thread, wakes at low watermark
  Scans inactive lists, frees clean pages, writes dirty
Direct reclaim: process that triggered low memory
  __alloc_pages_slowpath() → __perform_reclaim()
OOM killer: last resort, kills process with highest score
```

### WHEN: Page Moves Between Lists
```
T₁: New page → inactive list (PG_lru=1, PG_active=0)
T₂: Page accessed → PG_referenced=1 (not moved yet)
T₃: kswapd scans, sees referenced → move to active
T₄: No access → age out → PG_active=0, move to inactive
T₅: Still no access → reclaim candidate → free or swap
```

### WITHOUT: No Page Cache
```
Read 1000 × 4KB from same file:
  Without cache: 1000 disk reads × 5ms = 5000ms
  With cache: 1 disk read + 999 cache hits × 100ns = 5ms + 0.1ms
Speedup: 5000ms / 5.1ms = 980×
```

### WHICH: mlock Flags
```
VM_LOCKED = 0x00002000
VMA with vm_flags & VM_LOCKED = 1 → pages in LRU_UNEVICTABLE
mlock(ptr, 4096):
  Pages = ceil(4096 / 4096) = 1
  1 page moves to unevictable list
  Cannot be swapped, cannot be dropped
```

---

## ANNOYING CALCULATIONS — BREAKDOWN

### Annoying: Active Ratio
```
active_anon = 50000, inactive_anon = 150000
ratio = 50000 / (50000 + 150000) = 50000 / 200000 = 0.25 = 25%
Target ratio often ~50%, system is cold
```

### Annoying: Page Cache Size from /proc/meminfo
```
Cached: 3145728 kB = 3145728 / 4 = 786432 pages
Dirty: 12345 kB = 3087 pages
Clean in cache = 786432 - 3087 = 783345 pages
```

### Annoying: mlock Range
```
ptr = 0x7F0000001234, size = 100000 bytes
Start page = floor(0x7F0000001234 / 4096) = page at VA 0x7F0000001000
End addr = 0x7F0000001234 + 100000 = 0x7F00000199D4
End page = page containing 0x7F00000199D4 = 0x7F0000019000
Pages = (0x7F0000019000 - 0x7F0000001000) / 4096 + 1 = 25 pages
```

---

## ATTACK PLAN

```
1. LRU type from flags: PG_active, PG_lru, PG_unevictable
2. File page index = byte_offset / 4096
3. Reclaim order: inactive_file → inactive_anon → active
4. mlock pages = ceil((ptr_end - page_start) / 4096)
```

---

## ADDITIONAL FAILURE PREDICTIONS

```
FAILURE 7: mlock aligns to page boundaries → more pages than expected
FAILURE 8: Dirty pages need writeback before free → can't instant reclaim
FAILURE 9: Active ratio too low → system is memory-cold, lots of cold data
FAILURE 10: PG_referenced alone doesn't move page → needs kswapd scan
```

---

## SHELL COMMANDS — PARADOXICAL THINKING EXERCISES

### COMMAND 1: View LRU Statistics

```bash
cat /proc/vmstat | grep -E "^nr_|^pgpg|^pswp"

# WHAT: Page counts per LRU list, paging stats
# WHY: Understand memory pressure and reclaim activity
# WHERE: Per-node lruvec counters aggregated
# WHO: kswapd updates on reclaim, page fault updates on access
# WHEN: Continuously updated as pages move between lists
# WITHOUT: Blind OOM, no visibility into memory state
# WHICH: nr_active_anon, nr_inactive_anon, nr_active_file, nr_inactive_file

# EXAMPLE OUTPUT:
# nr_active_anon 125000
# nr_inactive_anon 375000
# nr_active_file 50000
# nr_inactive_file 150000
#
# CALCULATION:
# Active ratio (anon) = 125000 / (125000+375000) = 25%
# Inactive = 75% → lots of cold anonymous pages
#
# Total anonymous = (125000+375000) × 4KB = 2GB
# Total file = (50000+150000) × 4KB = 800MB
# Total tracked = 2GB + 800MB = 2.8GB
```

### COMMAND 2: Force Page Cache and Measure

```bash
# Clear cache and measure file read
sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
time cat /usr/lib/x86_64-linux-gnu/libc.so.6 > /dev/null
time cat /usr/lib/x86_64-linux-gnu/libc.so.6 > /dev/null  # Should be faster

# CALCULATION:
# libc.so.6 ≈ 2MB
# First read: disk → page cache → /dev/null
#   Disk: 2MB @ 500MB/s = 4ms
# Second read: page cache → /dev/null
#   RAM: 2MB @ 10GB/s = 0.2ms
#
# Speedup = 4ms / 0.2ms = 20×
#
# MEMORY DIAGRAM:
# ┌────────────────────────────────────────────────────────────────────┐
# │ Page Cache for libc.so.6                                           │
# │                                                                    │
# │ address_space @ 0xFFFF888112340000                                 │
# │ ├── host = inode for libc.so.6                                     │
# │ └── i_pages (xarray)                                               │
# │       ├── index 0 → page @ PFN 0x1000                              │
# │       ├── index 1 → page @ PFN 0x1001                              │
# │       ├── index 2 → page @ PFN 0x1002                              │
# │       └── ... (512 pages for 2MB file)                             │
# │                                                                    │
# │ Each page: 4KB, total = 512 × 4KB = 2MB cached                     │
# └────────────────────────────────────────────────────────────────────┘
```

### COMMAND 3: mlock Test

```bash
cat << 'EOF' > /tmp/mlock_test.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main() {
    size_t size = 10 * 1024 * 1024;  // 10MB
    void *ptr = malloc(size);
    
    printf("Before mlock: check VmLck in /proc/%d/status\n", getpid());
    system("grep VmLck /proc/$PPID/status");
    
    if (mlock(ptr, size) < 0) {
        perror("mlock");
        return 1;
    }
    
    printf("After mlock:\n");
    system("grep VmLck /proc/$PPID/status");
    
    munlock(ptr, size);
    free(ptr);
}
EOF
gcc /tmp/mlock_test.c -o /tmp/mlock_test && /tmp/mlock_test

# CALCULATION:
# size = 10MB = 10485760 bytes
# Pages = 10485760 / 4096 = 2560 pages
# mlock() will:
#   1. Fault in all 2560 pages if not present
#   2. Move all 2560 pages to LRU_UNEVICTABLE
#   3. Set VM_LOCKED on VMA
#   4. Increment Mlocked counter by 2560
#
# VmLck should show: 10240 kB (10MB)
```

### COMMAND 4: Trigger and Observe Reclaim

```bash
# Consume memory until reclaim happens
cat << 'EOF' > /tmp/pressure.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    for (int i = 0; i < 1000; i++) {
        void *p = malloc(10 * 1024 * 1024);  // 10MB
        memset(p, 'A', 10 * 1024 * 1024);    // Touch all pages
        printf("Allocated %d × 10MB = %dMB\n", i+1, (i+1)*10);
    }
}
EOF
gcc /tmp/pressure.c -o /tmp/pressure

# In one terminal: watch -n1 "grep -E 'Active|Inactive|Dirty' /proc/meminfo"
# In another: /tmp/pressure

# OBSERVATION:
# As memory fills:
# 1. Inactive(file) drops first (drop clean cache)
# 2. Dirty pages written (pgpgout increases)
# 3. Inactive(anon) starts swapping (pswpout increases)
# 4. Active lists shrink (demotion to inactive)
# 5. Eventually OOM killer activates
```

---

## FINAL PARADOX QUESTIONS

```
Q1: 8GB RAM, 4GB swap, should be 12GB usable?
    
    ANSWER:
    NO! Swap is 100× slower than RAM
    Random swap access: 10ms per page (SSD) vs 100ns (RAM)
    If 4GB in swap, 1GB accessed randomly:
      RAM: 1GB / 10GB/s = 100ms
      Swap: 1GB / 100MB/s = 10 seconds
    System unusable long before 12GB used
    
Q2: Why does kernel track "active" and "inactive" separately?
    
    CALCULATION:
    Single list with 1M pages, find coldest:
      Scan all 1M to find lowest access time = 1M comparisons
    
    Two lists:
      Inactive list = already cold
      Just scan inactive, ~500K pages
      Skip entire active list = 50% saved
    
Q3: Dirty page must be written before free. How long?
    
    CALCULATION:
    1 dirty page = 4KB
    HDD: 4KB @ 100MB/s = 40μs... BUT seek time = 10ms!
    Real: 10ms per random dirty page
    SSD: 4KB @ 500MB/s = 8μs, no seek
    
    1000 dirty pages:
      HDD sequential: 4MB @ 100MB/s = 40ms
      HDD random: 1000 × 10ms = 10 seconds!
      SSD: 4MB @ 500MB/s = 8ms
```
