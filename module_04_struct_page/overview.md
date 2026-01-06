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
