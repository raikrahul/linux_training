# Module 4: struct page Deep Dive

## Overview

This module explores the `struct page` structure, the kernel's metadata for every physical page. You will learn how the kernel packs multiple use-cases into overlapping union fields.

---

## 1. What is struct page?

Every physical page frame (4KB block of RAM) has a corresponding `struct page`:

```
Physical RAM:
┌──────────┬──────────┬──────────┬──────────┬──────────┐
│ Page 0   │ Page 1   │ Page 2   │ Page 3   │ ...      │
│ PFN=0    │ PFN=1    │ PFN=2    │ PFN=3    │          │
└──────────┴──────────┴──────────┴──────────┴──────────┘

struct page array (mem_map):
┌──────────┬──────────┬──────────┬──────────┬──────────┐
│ page[0]  │ page[1]  │ page[2]  │ page[3]  │ ...      │
└──────────┴──────────┴──────────┴──────────┴──────────┘
```

### Conversion Macros

```c
// Get struct page from PFN
struct page *page = pfn_to_page(pfn);

// Get PFN from struct page
unsigned long pfn = page_to_pfn(page);

// Get struct page from virtual address
struct page *page = virt_to_page(vaddr);

// Get virtual address from struct page
void *vaddr = page_address(page);
```

---

## 2. struct page Layout

```c
// include/linux/mm_types.h (simplified)
struct page {
    unsigned long flags;           // Page state flags
    
    union {
        struct {                   // Page cache / anonymous
            struct list_head lru;
            struct address_space *mapping;
            pgoff_t index;
            unsigned long private;
        };
        struct {                   // Slab allocator
            struct kmem_cache *slab_cache;
            void *freelist;
            int pages;
            int pobjects;
        };
        struct {                   // Compound page (huge page)
            unsigned long compound_head;
            unsigned char compound_dtor;
            unsigned char compound_order;
        };
    };
    
    atomic_t _refcount;            // Usage count
    atomic_t _mapcount;            // PTE mapping count
};
```

---

## 3. Page Flags

### Flag Layout in flags field

```
64-bit flags:
┌────────────────────┬─────────────┬──────────────────────────┐
│ Section/Spare (8)  │ Node+Zone(8)│ Actual Flags (48)        │
└────────────────────┴─────────────┴──────────────────────────┘
```

### Common Flags

```c
// include/linux/page-flags.h
enum pageflags {
    PG_locked,        // Page is locked for I/O
    PG_referenced,    // Page was recently accessed
    PG_uptodate,      // Page data is valid
    PG_dirty,         // Page has been modified
    PG_lru,           // Page is on LRU list
    PG_active,        // Page is on active LRU list
    PG_slab,          // Page is used by slab allocator
    PG_head,          // First page of compound page
    PG_tail,          // Tail page of compound page
    PG_swapbacked,    // Page has swap backing
    // ...
};
```

### Flag Access Macros

```c
// Test flag
if (PageLocked(page)) { ... }
if (PageDirty(page)) { ... }

// Set flag
SetPageDirty(page);
SetPageReferenced(page);

// Clear flag
ClearPageDirty(page);

// Test and set atomically
if (TestSetPageLocked(page)) { ... }
```

### Extract Zone and Node

```c
// Get zone from page flags
static inline enum zone_type page_zonenum(struct page *page)
{
    return (page->flags >> ZONES_PGSHIFT) & ZONES_MASK;
}

// Get node from page flags
static inline int page_to_nid(struct page *page)
{
    return (page->flags >> NODES_PGSHIFT) & NODES_MASK;
}
```

---

## 4. The mapping Field

The `mapping` field has multiple interpretations:

```
┌─────────────────────────────────────────────────────────────┐
│ mapping value                    │ Interpretation           │
├─────────────────────────────────────────────────────────────┤
│ NULL                             │ Anonymous, not mapped    │
│ ptr with LSB = 0                 │ File-backed page         │
│                                  │ Points to address_space  │
│ ptr with LSB = 1                 │ Anonymous mapped page    │
│                                  │ Points to anon_vma       │
│ ptr with LSB = 2                 │ KSM (merged) page        │
│ ptr with LSB = 3                 │ Movable page             │
└─────────────────────────────────────────────────────────────┘
```

### Decoding mapping

```c
// include/linux/page-flags.h
#define PAGE_MAPPING_ANON     0x1
#define PAGE_MAPPING_MOVABLE  0x2
#define PAGE_MAPPING_KSM      (PAGE_MAPPING_ANON | PAGE_MAPPING_MOVABLE)

static inline int PageAnon(struct page *page)
{
    return ((unsigned long)page->mapping & PAGE_MAPPING_ANON) != 0;
}

static inline struct address_space *page_mapping(struct page *page)
{
    unsigned long mapping = (unsigned long)page->mapping;
    
    if (mapping & PAGE_MAPPING_ANON)
        return NULL;  // Anonymous page has no address_space
    
    return (struct address_space *)(mapping & ~PAGE_MAPPING_FLAGS);
}
```

---

## 5. Reference Counting

### Two Counters

```
_refcount:  Total references to this page
            Starts at 1 when allocated
            0 means page is free

_mapcount:  Number of page table entries pointing here
            -1 when not mapped
            0 when mapped by one PTE
            >0 when mapped by multiple PTEs (shared)
```

### Reference Counting API

```c
// Increment reference
get_page(page);          // _refcount++
page_ref_inc(page);      // Same thing

// Decrement and possibly free
put_page(page);          // _refcount--, free if zero

// Get reference count
int count = page_ref_count(page);

// Map count
int mapcount = page_mapcount(page);
```

### Example: Page Lifecycle

```
1. alloc_page(GFP_KERNEL)
   _refcount = 1, _mapcount = -1
   
2. Page is mapped into process A
   _refcount = 2, _mapcount = 0
   
3. fork() creates process B with same mapping
   _refcount = 3, _mapcount = 1
   
4. Process A unmaps the page
   _refcount = 2, _mapcount = 0
   
5. Process B unmaps the page
   _refcount = 1, _mapcount = -1
   
6. __free_pages() called
   _refcount = 0 → page returned to buddy
```

---

## 6. Compound Pages (Huge Pages)

### Structure

```
Compound Page (order=1, 8KB):
┌─────────────────┬─────────────────┐
│ Head Page       │ Tail Page       │
│ compound_head=0 │ compound_head   │
│ compound_order=1│ = &head | 1     │
│ compound_dtor=X │                 │
└─────────────────┴─────────────────┘
```

### Code

```c
// Check if compound page
if (PageCompound(page)) {
    struct page *head = compound_head(page);
    int order = compound_order(head);
    pr_info("Compound page, order %d (%ld bytes)\n",
            order, PAGE_SIZE << order);
}

// Get head from any page in compound
static inline struct page *compound_head(struct page *page)
{
    unsigned long head = READ_ONCE(page->compound_head);
    
    if (unlikely(head & 1))
        return (struct page *)(head - 1);
    return page;
}
```

---

## 7. Kernel Module Example

```c
// page_info.c
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/gfp.h>

static int __init page_info_init(void)
{
    struct page *page;
    unsigned long pfn;
    
    // Allocate a page
    page = alloc_page(GFP_KERNEL);
    if (!page)
        return -ENOMEM;
    
    pfn = page_to_pfn(page);
    
    pr_info("=== Page Info ===\n");
    pr_info("struct page at: %px\n", page);
    pr_info("PFN: 0x%lx\n", pfn);
    pr_info("Physical addr: 0x%lx\n", pfn << PAGE_SHIFT);
    pr_info("Virtual addr: %px\n", page_address(page));
    
    pr_info("\n=== Flags ===\n");
    pr_info("flags raw: 0x%lx\n", page->flags);
    pr_info("Zone: %d\n", page_zonenum(page));
    pr_info("Node: %d\n", page_to_nid(page));
    pr_info("PG_locked: %d\n", PageLocked(page));
    pr_info("PG_lru: %d\n", PageLRU(page));
    
    pr_info("\n=== Counts ===\n");
    pr_info("_refcount: %d\n", page_ref_count(page));
    pr_info("_mapcount: %d\n", page_mapcount(page));
    
    pr_info("\n=== Mapping ===\n");
    pr_info("mapping: %px\n", page->mapping);
    pr_info("PageAnon: %d\n", PageAnon(page));
    
    // Clean up
    __free_page(page);
    
    return 0;
}

static void __exit page_info_exit(void)
{
    pr_info("page_info module unloaded\n");
}

module_init(page_info_init);
module_exit(page_info_exit);
MODULE_LICENSE("GPL");
```

### Sample Output

```
=== Page Info ===
struct page at: ffff888102340080
PFN: 0x123400
Physical addr: 0x123400000
Virtual addr: ffff888123400000

=== Flags ===
flags raw: 0x17ffffc0000000
Zone: 2
Node: 0
PG_locked: 0
PG_lru: 0

=== Counts ===
_refcount: 1
_mapcount: -1

=== Mapping ===
mapping: 0000000000000000
PageAnon: 0
```

---

## 8. Practice Exercises

### Exercise 1: Flag Extraction

Given `page->flags = 0x17ffffc0010068`, extract:
- Zone number
- Node number  
- Which PG_* flags are set?

### Exercise 2: Mapping Decode

Write code that:
1. Gets a page from a file mmap
2. Reads page->mapping
3. Determines if anonymous or file-backed
4. If file-backed, prints the inode number

### Exercise 3: Refcount Tracing

Create a kprobe that tracks get_page/put_page calls for a specific PFN.

---

## Next Module

[Module 5: Advanced Memory Topics →](../module_05_advanced_memory/)

[← Back to Course Index](../index.md)

---

## AXIOMATIC EXERCISES — BRUTE FORCE CALCULATION

### EXERCISE A: PFN TO STRUCT PAGE

```
GIVEN:
  Physical address = 0x12345678
  PAGE_SHIFT = 12
  mem_map base = 0xFFFF_EA00_0000_0000
  sizeof(struct page) = 64 bytes

TASK:

1. PFN = PA >> PAGE_SHIFT = 0x12345678 >> 12 = 0x___
2. page offset = PFN × sizeof(struct page) = 0x___ × 64 = 0x___
3. struct page address = mem_map + offset = 0xFFFF_EA00_0000_0000 + 0x___ = 0x___

VERIFY: page_to_pfn(page) should return 0x___
```

### EXERCISE B: FLAG EXTRACTION

```
GIVEN: page->flags = 0x17FFFFC0_00014068

TASK: Extract zone and node (assuming ZONES_SHIFT=2, NODES_SHIFT=6)

1. flags in binary = ___________________________________
2. Zone bits at position [ZONES_PGSHIFT : ZONES_PGSHIFT+2] = ___
3. Zone number = ___ → ZONE_DMA32 / ZONE_NORMAL?
4. Node bits at position [NODES_PGSHIFT : NODES_PGSHIFT+6] = ___
5. Node number = ___

PAGE FLAGS (low bits):
6. bit 0 (PG_locked) = ___
7. bit 3 (PG_referenced) = ___
8. bit 5 (PG_uptodate) = ___
9. bit 6 (PG_lru) = ___
10. bit 13 (PG_active) = ___
```

### EXERCISE C: MAPPING FIELD DECODE

```
GIVEN: page->mapping values, determine type:

1. mapping = 0x0000_0000_0000_0000 → ___ (NULL)
2. mapping = 0xFFFF_8881_1234_5000 → LSB = ___ → ___-backed
3. mapping = 0xFFFF_8881_1234_5001 → LSB = ___ → ___
4. mapping = 0xFFFF_8881_1234_5002 → LSB = ___ → ___ (movable)
5. mapping = 0xFFFF_8881_1234_5003 → LSB = ___ → ___ (KSM)

DECODE:
  LSB & 1 = 1 → anonymous (anon_vma pointer)
  LSB & 2 = 2 → movable
  LSB & 3 = 3 → KSM merged
  LSB = 0 → file-backed (address_space pointer)
```

### EXERCISE D: REFCOUNT TRACKING

```
GIVEN: Scenario timeline

T1: alloc_page() → page allocated
    _refcount = ___, _mapcount = ___

T2: Page mapped into process A's page table
    _refcount = ___, _mapcount = ___

T3: fork() creates process B, same page shared
    _refcount = ___, _mapcount = ___

T4: Process A unmaps the page
    _refcount = ___, _mapcount = ___

T5: Process B unmaps the page
    _refcount = ___, _mapcount = ___

T6: Page freed
    _refcount = ___, page returned to ___
```

### EXERCISE E: COMPOUND PAGE NAVIGATION

```
GIVEN: Compound page order=2 (4 pages), head at PFN 0x1000

TASK:

┌──────────┬──────────┬──────────┬──────────┐
│ Page 0   │ Page 1   │ Page 2   │ Page 3   │
│ PFN=___  │ PFN=___  │ PFN=___  │ PFN=___  │
│ HEAD     │ TAIL     │ TAIL     │ TAIL     │
└──────────┴──────────┴──────────┴──────────┘

1. Head page PFN = 0x___
2. compound_order = ___
3. Tail page 1 compound_head = &head | 1 = ___
4. From tail page 2, get head: compound_head & ~1 = ___
5. Total size = 2^order × 4096 = ___ bytes
```

---

## FAILURE PREDICTIONS

```
FAILURE 1: sizeof(struct page) varies, not always 64 → wrong offset
FAILURE 2: Zone/node bit positions depend on config → extract wrong bits
FAILURE 3: LSB=1 means anonymous, not LSB!=0 → confuse with movable
FAILURE 4: _mapcount starts at -1, not 0 → off-by-one
FAILURE 5: compound_head has bit 0 set in tail pages → must mask
FAILURE 6: _refcount=0 doesn't mean immediate free, may be slab-owned
```

---

## W-QUESTIONS — NUMERICAL ANSWERS

### WHAT: struct page Size
```
sizeof(struct page) = 64 bytes on typical x86_64
16GB RAM = 16 × 2^30 bytes
Pages = 16GB / 4KB = 4194304 pages
mem_map size = 4194304 × 64 = 268435456 bytes = 256MB
∴ 1.5% of RAM used for page metadata
```

### WHY: Refcount Separate from Mapcount
```
Page in page cache, not mapped: refcount=1, mapcount=-1
Page mapped by 2 processes: refcount=3, mapcount=1
refcount = all references (cache + mappings + temp)
mapcount = PTE count only
Free when refcount=0, not when mapcount=-1
```

### WHERE: mem_map Lives
```
Node 0: mem_map at 0xFFFF_EA00_0000_0000
Node 1: mem_map at 0xFFFF_EA00_0400_0000 (offset by node)
PFN 0x1234 on node 0:
  page address = 0xFFFF_EA00_0000_0000 + 0x1234 × 64
               = 0xFFFF_EA00_0000_0000 + 0x48D00
               = 0xFFFF_EA00_0004_8D00
```

### WHO: Modifies Refcount
```
get_page(): refcount 1→2 (module takes reference)
put_page(): refcount 2→1 (module releases)
Map into PTE: mapcount -1→0 (first mapping)
fork(): mapcount 0→1 (second process maps)
munmap(): mapcount 1→0→unmapped
```

### WHEN: Compound Page Created
```
alloc_pages(GFP_KERNEL, 2) → order-2 = 4 pages
Page 0 (head): compound_order=2, compound_dtor set
Page 1 (tail): compound_head = &page0 | 1
Page 2 (tail): compound_head = &page0 | 1
Page 3 (tail): compound_head = &page0 | 1
Access page2→compound_head() returns page0
```

### WITHOUT: No Page Flags
```
Without flags:
  - Cannot tell if page is dirty → write entire cache
  - Cannot tell if page locked → race conditions
  - Cannot tell if on LRU → memory leak
32-bit flags field, but only bits[0:23] for page flags
Upper bits: node, zone, section encoding
```

### WHICH: Mapping Type
```
mapping & 3 = 0 → file-backed (address_space *)
mapping & 3 = 1 → anonymous (anon_vma *)
mapping & 3 = 2 → movable migration
mapping & 3 = 3 → KSM merged page
Example: 0xFFFF888112345001 & 3 = 1 → anonymous
```

---

## ANNOYING CALCULATIONS — BREAKDOWN

### Annoying: PFN to page Address
```
mem_map = 0xFFFF_EA00_0000_0000
PFN = 0xABCD
page_size = 64 bytes
offset = 0xABCD × 64 = 0xABCD × 0x40 = 0x2AF340
page_addr = 0xFFFF_EA00_0000_0000 + 0x2AF340 = 0xFFFF_EA00_002A_F340
```

### Annoying: Extract Zone from Flags
```
flags = 0x17FFFFC0_00014068
ZONES_PGSHIFT = 60 (depends on config)
zone_bits = (flags >> 60) & 0x3 = (0x17FFFFC) >> 4 & 0x3 = 0x1 & 0x3 = 1
zone 1 = ZONE_DMA32 on typical config
```

### Annoying: Check PageLRU
```
PG_lru = bit 6
flags = 0x14068
bit 6 = (0x14068 >> 6) & 1 = (0x501) & 1 = 1
∴ PageLRU = 1, page is on LRU list
```

### Annoying: Mapcount Interpretation
```
mapcount = -1 → no PTEs → 0 mappings
mapcount = 0 → 1 PTE → 1 mapping (off by one!)
mapcount = 1 → 2 PTEs → 2 mappings
Real mappings = mapcount + 1 (when mapcount >= 0)
```

---

## ATTACK PLAN

```
1. page_addr = mem_map + PFN × sizeof(struct page)
2. zone = (flags >> ZONES_PGSHIFT) & ZONES_MASK
3. PageXXX = (flags >> PG_XXX) & 1
4. mapping type = mapping & 3
5. mappings = mapcount + 1 (if mapcount >= 0)
```

---

## ADDITIONAL FAILURE PREDICTIONS

```
FAILURE 7: sizeof(struct page) varies by config, not always 64
FAILURE 8: mapcount=-1 means 0 mappings, mapcount=0 means 1 → off-by-one
FAILURE 9: Zone bits position depends on CONFIG_SPARSEMEM
FAILURE 10: compound_head has bit 0 set in tail → must mask with ~1
```

---

## SHELL COMMANDS — PARADOXICAL THINKING EXERCISES

### COMMAND 1: Calculate mem_map Size

```bash
# Get total memory and calculate struct page overhead
TOTAL_KB=$(grep MemTotal /proc/meminfo | awk '{print $2}')
TOTAL_PAGES=$((TOTAL_KB / 4))
PAGE_STRUCT_SIZE=64  # bytes, typical
MEMMAP_SIZE=$((TOTAL_PAGES * PAGE_STRUCT_SIZE))
MEMMAP_MB=$((MEMMAP_SIZE / 1024 / 1024))

echo "Total RAM: $((TOTAL_KB/1024)) MB"
echo "Total pages: $TOTAL_PAGES"
echo "mem_map size: $MEMMAP_MB MB"
echo "Overhead: $(echo "scale=2; $MEMMAP_MB * 100 / ($TOTAL_KB/1024)" | bc)%"

# CALCULATION for 16GB:
# Total pages = 16GB / 4KB = 16 × 2^30 / 4 × 2^10 = 4 × 2^20 = 4194304 pages
# mem_map size = 4194304 × 64 = 268435456 bytes = 256MB
# Overhead = 256MB / 16384MB = 1.56%
#
# SCALE:
# 1GB RAM: 262144 pages × 64 = 16MB overhead (1.56%)
# 64GB RAM: 16777216 pages × 64 = 1GB overhead (1.56%)
# 1TB RAM: 268435456 pages × 64 = 16GB overhead (1.56%)
#
# PARADOX: Overhead is constant percentage regardless of RAM size!
```

### COMMAND 2: Explore Page Flags

```bash
# Read kernel page flags documentation
cat /sys/kernel/debug/kernel_page_owner 2>/dev/null | head -1 || \
echo "page_owner not enabled, try: zcat /usr/src/linux/Documentation/admin-guide/mm/pagemap.rst.gz"

# PAGE FLAGS BIT LAYOUT (from kernel source):
# ┌────────────────────────────────────────────────────────────────────┐
# │ page->flags (64 bits)                                              │
# │                                                                    │
# │ bit  0: PG_locked      │ bit 16: PG_reclaim                       │
# │ bit  1: PG_writeback   │ bit 17: PG_swapbacked                    │
# │ bit  2: PG_referenced  │ bit 18: PG_unevictable                   │
# │ bit  3: PG_uptodate    │ bit 19: PG_mlocked                       │
# │ bit  4: PG_dirty       │ ...                                      │
# │ bit  5: PG_lru         │                                          │
# │ bit  6: PG_active      │ bits 58-63: zone/node encoding           │
# │ bit  7: PG_waiters     │                                          │
# │ bit  8: PG_slab        │                                          │
# └────────────────────────────────────────────────────────────────────┘
#
# CALCULATION:
# flags = 0x14068
# Binary: 0001 0100 0000 0110 1000
#   bit 3 = 1 → PG_uptodate ✓
#   bit 5 = 1 → PG_lru ✓
#   bit 6 = 1 → PG_active ✓
#   bit 14 = 1 → PG_mappedtodisk ✓
```

### COMMAND 3: Read /proc/kpageflags

```bash
# Read raw page flags for a PFN (requires root)
PFN=0x12345
sudo dd if=/proc/kpageflags bs=8 skip=$((PFN)) count=1 2>/dev/null | xxd

# WHAT: 64-bit flags per PFN, exported to userspace
# WHY: allows memory analysis tools
# WHERE: /proc/kpageflags at offset PFN×8
# WHO: kernel exports, tools like page-types read
# WHEN: read triggers kernel to look up struct page
# WITHOUT: need kernel module to read page->flags
# WHICH: bit layout differs from internal page->flags!

# CALCULATION:
# PFN = 0x12345 = 74565 decimal
# File offset = 74565 × 8 = 596520 bytes
# dd skip=74565 reads from byte 596520
```

### COMMAND 4: Observe Refcount Changes

```bash
# Use kernel tracepoints for get_page/put_page
sudo sh -c 'echo 1 > /sys/kernel/debug/tracing/events/kmem/mm_page_alloc/enable'
sudo cat /sys/kernel/debug/tracing/trace | tail -10

# REFCOUNT LIFECYCLE:
# ┌─────────────────────────────────────────────────────────────────┐
# │ T1: alloc_page() called                                         │
# │     page = buddy_alloc()                                        │
# │     page->_refcount = 1                                         │
# │     page->_mapcount = -1 (not mapped)                           │
# │                                                                 │
# │ T2: Page mapped by mmap                                         │
# │     set_pte() installs PTE                                      │
# │     page_add_file_rmap() or page_add_anon_rmap()               │
# │     page->_refcount = 2 (alloc + mapping)                       │
# │     page->_mapcount = 0 (one PTE)                               │
# │                                                                 │
# │ T3: fork() creates child                                        │
# │     copy_pte_range() copies PTEs                                │
# │     page_dup_rmap()                                             │
# │     page->_refcount = 3                                         │
# │     page->_mapcount = 1 (two PTEs)                              │
# │                                                                 │
# │ T4: Child exits                                                 │
# │     zap_pte_range() removes child PTE                           │
# │     page_remove_rmap()                                          │
# │     page->_refcount = 2                                         │
# │     page->_mapcount = 0                                         │
# └─────────────────────────────────────────────────────────────────┘
```

---

## FINAL PARADOX QUESTIONS

```
Q1: _mapcount = -1 means 0 mappings. Why not just use 0?
    
    ANSWER:
    page_mapcount(page) returns mapcount + 1
    -1 + 1 = 0 → 0 mappings
    0 + 1 = 1 → 1 mapping
    This avoids special case for "not mapped"
    Any mapcount >= 0 means "at least one mapping"
    
Q2: Why is struct page 64 bytes, not smaller?
    
    CALCULATION:
    Needed fields: flags(8) + lru(16) + mapping(8) + index(8) + refcount(4) + mapcount(4)
    Minimum = 48 bytes
    But: cache line = 64 bytes
    Padding to 64 eliminates false sharing between pages
    256MB overhead for 16GB vs saving 12 bytes/page = 48MB
    Cache efficiency > memory savings
    
Q3: compound_head has bit 0 set in tail pages. How does that work?
    
    ANSWER:
    Head page: compound_head = 0 or points to itself
    Tail page: compound_head = &head_page | 1
    To get head: mask off bit 0: (compound_head & ~1)
    Bit 0 = 1 indicates "this is a tail page"
    Saves a separate flag bit in page->flags
```



## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: REFCOUNT/MAPCOUNT LOGIC
START: ALLOC_PAGE() → PFN=0x500

M1. INITIAL_STATE:
PAGE_ADDR = MEM_MAP + (0x500 * 64)
flags = 0
_refcount = 1
_mapcount = -1

M2. PROCESS_A_MAP:
PTE_A[0x10] = PFN_0x500
PAGE_ADD_RMAP()
_refcount: 1 → 2 (1 for alloc + 1 for map) (Wait, alloc ref consumed? No)
Logic: Alloc=1. Map=1. Total=2.
_mapcount: -1 → 0 (1 mapping)

M3. FORK_PROCESS_B:
SIZE 1GB NOT COPY. PTE COPY ONLY.
PTE_B[0x10] = PFN_0x500
PAGE_DUP_RMAP()
_refcount: 2 → 3
_mapcount: 0 → 1 (2 mappings)

M4. PROCESS_A_UNMAP:
ZAP_PTE(PTE_A)
PAGE_REMOVE_RMAP()
_refcount: 3 → 2
_mapcount: 1 → 0

M5. PROCESS_B_EXIT:
ZAP_PTE(PTE_B)
PAGE_REMOVE_RMAP()
_refcount: 2 → 1
_mapcount: 0 → -1

M6. FREE_PAGE:
PUT_PAGE()
_refcount: 1 → 0
IF 0 → RETURN_TO_BUDDY ✓


---

[← Previous Lesson](../module_03_allocators/lesson_03.md) | [Course Index](../index.md) | [Next Lesson →](../module_05_advanced_memory/lesson_05.md)
