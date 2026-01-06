# Module 3: Memory Allocators

## Overview

This module covers the Linux kernel memory allocation subsystem. You will understand how the buddy allocator manages physical pages and how the slab allocator provides efficient object caching.

---

## 1. Memory Allocation Layers

```
┌─────────────────────────────────────────────────────────┐
│                    User Space                            │
│    malloc() / free() / mmap()                           │
└─────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                    Kernel Space                          │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │           SLAB Allocator (SLUB)                  │   │
│  │    kmalloc() / kfree() / kmem_cache_alloc()      │   │
│  └──────────────────────────────────────────────────┘   │
│                         │                                │
│                         ▼                                │
│  ┌──────────────────────────────────────────────────┐   │
│  │              Buddy Allocator                      │   │
│  │    alloc_pages() / __free_pages()                │   │
│  └──────────────────────────────────────────────────┘   │
│                         │                                │
│                         ▼                                │
│  ┌──────────────────────────────────────────────────┐   │
│  │           Physical Memory (RAM)                   │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

---

## 2. Buddy Allocator

### The Problem: External Fragmentation

If we allocate and free randomly sized blocks, we get fragmented free space:

```
Memory: [USED][FREE][USED][FREE][FREE][USED][FREE]
                16KB       8KB  8KB         4KB

Need 32KB? Can't satisfy even though 36KB total is free!
```

### The Solution: Power-of-2 Blocks

All allocations are power-of-2 sized:

```
Order 0:  4KB   (2^0 = 1 page)
Order 1:  8KB   (2^1 = 2 pages)
Order 2:  16KB  (2^2 = 4 pages)
Order 3:  32KB  (2^3 = 8 pages)
...
Order 10: 4MB   (2^10 = 1024 pages)
```

### Buddy System Data Structure

```c
// mm/page_alloc.c
struct zone {
    struct free_area free_area[MAX_ORDER];  // MAX_ORDER = 11
    // ...
};

struct free_area {
    struct list_head free_list[MIGRATE_TYPES];
    unsigned long nr_free;
};
```

```
Zone free lists:
┌─────────────────────────────────────────────────────────┐
│ Order 0:  [page]→[page]→[page]→NULL   nr_free=3        │
│ Order 1:  [pair]→[pair]→NULL          nr_free=2        │
│ Order 2:  [quad]→NULL                 nr_free=1        │
│ Order 3:  NULL                        nr_free=0        │
│ ...                                                     │
└─────────────────────────────────────────────────────────┘
```

### Allocation Algorithm

```
Request: 12KB
         │
         ▼
Step 1: Calculate order
        12KB → round up to 16KB → order=2 (2^2 × 4KB = 16KB)
         │
         ▼
Step 2: Check free_area[2]
        Empty? Go to higher order
         │
         ▼
Step 3: Check free_area[3] (32KB)
        Found block!
         │
         ▼
Step 4: Split the block
        32KB block splits into:
        - 16KB block (returned to caller)
        - 16KB block (added to free_area[2])
```

### Splitting Diagram

```
Before allocation (request 16KB):
Order 3: [████████████████████████████████]  32KB block

After split:
Order 2: [████████████████]  16KB (ALLOCATED)
Order 2: [░░░░░░░░░░░░░░░░]  16KB (FREE, added to free list)
```

### Free Algorithm (Coalescing)

```
Free 16KB block at address A:
         │
         ▼
Step 1: Calculate buddy address
        buddy_addr = A XOR (1 << (order + PAGE_SHIFT))
         │
         ▼
Step 2: Is buddy free?
        ├─ NO:  Add block to free_area[order], done
        └─ YES: Remove buddy from free list
                Coalesce into larger block
                Repeat from step 1 with higher order
```

### Buddy Address Calculation

```c
// mm/internal.h
static inline unsigned long
find_buddy_pfn(unsigned long pfn, unsigned int order)
{
    return pfn ^ (1 << order);
}
```

Example:
```
PFN = 0x1234 = 0b0001001000110100
Order = 2

Buddy PFN = 0x1234 XOR (1 << 2)
          = 0x1234 XOR 0x4
          = 0b0001001000110100 XOR 0b0000000000000100
          = 0b0001001000110000
          = 0x1230
```

---

## 3. Viewing Buddy State

### /proc/buddyinfo

```bash
$ cat /proc/buddyinfo
Node 0, zone    DMA      1      0      0      1      2      1      1      0      1      1      3
Node 0, zone  DMA32   3912   3015   2107   1293    624    243     82     24      8      3     67
Node 0, zone Normal  12851   8742   5211   2834   1203    412    127     38     11      2    142
```

Format: free blocks at each order (0-10)

### Reading Code

```c
// userspace: read_buddy.c
#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *f = fopen("/proc/buddyinfo", "r");
    char line[256];
    
    while (fgets(line, sizeof(line), f)) {
        char node[32], zone[32];
        int orders[11];
        
        sscanf(line, "%s %*s %s %d %d %d %d %d %d %d %d %d %d %d",
               node, zone,
               &orders[0], &orders[1], &orders[2], &orders[3],
               &orders[4], &orders[5], &orders[6], &orders[7],
               &orders[8], &orders[9], &orders[10]);
        
        printf("%s %s: ", node, zone);
        for (int i = 0; i < 11; i++) {
            printf("Order%d=%d ", i, orders[i]);
        }
        printf("\n");
    }
    fclose(f);
    return 0;
}
```

---

## 4. Slab Allocator

### The Problem: Internal Fragmentation

Buddy allocator minimum is 4KB. What if you need 64 bytes?

```
Request 64 bytes:
Buddy gives 4KB page
Only use 64 bytes
Waste: 4096 - 64 = 4032 bytes (98.4% wasted!)
```

### The Solution: Object Caching

```
┌─────────────────────────────────────────────────────────┐
│                    Slab Cache                            │
│    "task_struct cache" - each object = 4KB              │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │  Slab (one or more pages from buddy)              │ │
│  │  ┌────────┬────────┬────────┬────────┬────────┐   │ │
│  │  │ obj[0] │ obj[1] │ obj[2] │ obj[3] │ obj[4] │   │ │
│  │  └────────┴────────┴────────┴────────┴────────┘   │ │
│  └────────────────────────────────────────────────────┘ │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │  Another Slab                                      │ │
│  │  ┌────────┬────────┬────────┬────────┬────────┐   │ │
│  │  │ FREE   │ FREE   │ obj[7] │ FREE   │ obj[9] │   │ │
│  │  └────────┴────────┴────────┴────────┴────────┘   │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### kmem_cache API

```c
// Creating a cache
struct kmem_cache *my_cache;

my_cache = kmem_cache_create(
    "my_objects",         // Name (visible in /proc/slabinfo)
    sizeof(struct my_obj), // Object size
    0,                     // Alignment (0 = default)
    SLAB_HWCACHE_ALIGN,    // Flags
    NULL                   // Constructor (optional)
);

// Allocating from cache
struct my_obj *obj = kmem_cache_alloc(my_cache, GFP_KERNEL);

// Freeing to cache
kmem_cache_free(my_cache, obj);

// Destroying cache
kmem_cache_destroy(my_cache);
```

### kmalloc Sizes

kmalloc uses predefined slab caches:

```c
// include/linux/slab.h
enum kmalloc_cache_type {
    KMALLOC_NORMAL = 0,
    KMALLOC_CGROUP,
    KMALLOC_RECLAIM,
    KMALLOC_DMA,
};

// Size classes: 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192

// Example allocation
void *p = kmalloc(100, GFP_KERNEL);
// Actually allocates 128 bytes (next power of 2)
```

### /proc/slabinfo

```bash
$ cat /proc/slabinfo
# name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab>
task_struct           1524       1564      4352        7           8
mm_struct             1015       1050      1600       10           4
vm_area_struct        43296     44268       200       20           1
dentry               123456    124000       192       21           1
inode_cache           52315     52400       608       13           2
kmalloc-256           12543     12800       256       16           1
kmalloc-128            8962      9120       128       32           1
```

---

## 5. GFP Flags

### Common Flags

```c
// include/linux/gfp.h

// Can sleep, can do I/O, most permissive
GFP_KERNEL    // = __GFP_RECLAIM | __GFP_IO | __GFP_FS

// Cannot sleep (interrupt/atomic context)
GFP_ATOMIC    // = __GFP_HIGH | __GFP_NOWARN

// Userspace allocation
GFP_USER      // = __GFP_RECLAIM | __GFP_IO | __GFP_FS | __GFP_HARDWALL

// DMA-capable memory (low 16MB)
GFP_DMA       // = __GFP_DMA

// Zero the allocated memory
__GFP_ZERO
```

### Context Rules

```
Context                          Allowed GFP flags
─────────────────────────────────────────────────────
Process context, no locks        GFP_KERNEL
Process context, spinlock held   GFP_ATOMIC
Interrupt handler                GFP_ATOMIC
Softirq                          GFP_ATOMIC
Workqueue                        GFP_KERNEL
```

### Danger Example

```c
// WRONG: Will deadlock!
spin_lock(&my_lock);
ptr = kmalloc(100, GFP_KERNEL);  // Can sleep while holding spinlock!
spin_unlock(&my_lock);

// CORRECT:
spin_lock(&my_lock);
ptr = kmalloc(100, GFP_ATOMIC);  // Cannot sleep
spin_unlock(&my_lock);

// BEST: Allocate outside lock
ptr = kmalloc(100, GFP_KERNEL);
spin_lock(&my_lock);
// use ptr
spin_unlock(&my_lock);
```

---

## 6. Kernel Module Example

```c
// alloc_demo.c
#include <linux/module.h>
#include <linux/slab.h>

static struct kmem_cache *demo_cache;

struct demo_object {
    int id;
    char data[60];  // Total: 64 bytes
};

static int __init alloc_demo_init(void)
{
    struct demo_object *obj1, *obj2;
    void *kmalloc_ptr;
    struct page *pages;
    
    // Create slab cache
    demo_cache = kmem_cache_create("demo_objects",
                                    sizeof(struct demo_object),
                                    0, 0, NULL);
    if (!demo_cache)
        return -ENOMEM;
    
    // Allocate from cache
    obj1 = kmem_cache_alloc(demo_cache, GFP_KERNEL);
    obj2 = kmem_cache_alloc(demo_cache, GFP_KERNEL);
    
    pr_info("obj1 at %px, obj2 at %px\n", obj1, obj2);
    pr_info("Distance: %ld bytes\n", (char*)obj2 - (char*)obj1);
    
    // kmalloc example
    kmalloc_ptr = kmalloc(100, GFP_KERNEL);
    pr_info("kmalloc(100) at %px, actual size: %zu\n",
            kmalloc_ptr, ksize(kmalloc_ptr));
    
    // Direct page allocation
    pages = alloc_pages(GFP_KERNEL, 2);  // 4 pages (16KB)
    pr_info("alloc_pages(order=2) PFN: %lx\n", page_to_pfn(pages));
    
    // Cleanup
    kmem_cache_free(demo_cache, obj1);
    kmem_cache_free(demo_cache, obj2);
    kfree(kmalloc_ptr);
    __free_pages(pages, 2);
    
    return 0;
}

static void __exit alloc_demo_exit(void)
{
    kmem_cache_destroy(demo_cache);
}

module_init(alloc_demo_init);
module_exit(alloc_demo_exit);
MODULE_LICENSE("GPL");
```

---

## 7. Practice Exercises

### Exercise 1: Buddy Math

A zone has 1GB of memory. Calculate:
- Total pages: ?
- If all pages are free at order 10, how many order-10 blocks?
- If we allocate 10 order-3 blocks, what changes?

### Exercise 2: Fragmentation Analysis

Write a program that:
1. Reads /proc/buddyinfo
2. Calculates total free memory
3. Calculates largest allocatable block
4. Reports fragmentation percentage

### Exercise 3: Slab Cache Monitor

Create a kernel module that:
1. Creates a custom slab cache
2. Allocates/frees objects in a pattern
3. Prints cache statistics

---

## Next Module

[Module 4: struct page Deep Dive →](../module_04_struct_page/)

[← Back to Course Index](../index.md)
