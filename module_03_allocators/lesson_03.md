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

---

## AXIOMATIC EXERCISES — BRUTE FORCE CALCULATION

### EXERCISE A: ORDER CALCULATION

```
GIVEN: Need to allocate 27000 bytes

TASK:

1. Pages needed = ceil(27000 / 4096) = ceil(___) = ___ pages
2. Next power of 2 ≥ ___ pages = ___ pages
3. Order = log2(___) = ___
4. Actual allocation = 2^___ × 4096 = ___ bytes
5. Internal fragmentation = ___ - 27000 = ___ bytes = ___% wasted
```

### EXERCISE B: BUDDY SPLIT

```
GIVEN: Request order-2 (16KB), only order-4 (64KB) available

TASK: Show split chain

┌────────────────────────────────────────────────────────────┐
│ Order-4 block at PFN 0x1000 (64KB)                        │
└────────────────────────────────────────────────────────────┘

SPLIT 1: Order-4 → two Order-3
┌────────────────────────────┬────────────────────────────┐
│ Order-3 at PFN ___         │ Order-3 at PFN ___         │
│ (___KB)                    │ (___KB) → to free_area[3]  │
└────────────────────────────┴────────────────────────────┘

SPLIT 2: Order-3 → two Order-2
┌─────────────┬─────────────┐
│ Order-2     │ Order-2     │
│ PFN ___     │ PFN ___     │
│ RETURNED    │ free_area[2]│
└─────────────┴─────────────┘

CALCULATE PFNs: 
  PFN difference between buddies at order N = 2^N
```

### EXERCISE C: BUDDY ADDRESS CALCULATION

```
GIVEN: PFN = 0x1234, order = 3

TASK:

1. Buddy PFN = PFN XOR (1 << order)
2. = 0x1234 XOR (1 << 3)
3. = 0x1234 XOR 0x___
4. = 0b_______________ XOR 0b_______________
5. = 0b_______________
6. = 0x___

VERIFY: 0x1234 XOR 0x___ = 0x___ (original PFN) ✓
```

### EXERCISE D: SLAB OBJECT COUNT

```
GIVEN:
  Slab page size = 4096 bytes
  Object size = 192 bytes
  Metadata per slab = 64 bytes

TASK:

1. Available for objects = 4096 - 64 = ___ bytes
2. Objects per slab = floor(___ / 192) = ___
3. Actual used = ___ × 192 = ___ bytes
4. Wasted per slab = ___ - ___ = ___ bytes
```

### EXERCISE E: KMALLOC SIZE CLASS

```
GIVEN: kmalloc(100, GFP_KERNEL)

TASK:

1. Size classes: 8, 16, 32, 64, 128, 256, 512...
2. Smallest class ≥ 100 = ___
3. Internal fragmentation = ___ - 100 = ___ bytes = ___% wasted

REPEAT FOR:
  kmalloc(1) → class ___, waste ___ bytes
  kmalloc(33) → class ___, waste ___ bytes
  kmalloc(4097) → class ___ or alloc_pages?
```

### EXERCISE F: GFP FLAG CONTEXT

```
GIVEN scenarios, fill correct GFP flag:

1. Process context, no locks held → GFP___
2. Spinlock held → GFP___
3. Interrupt handler → GFP___
4. softirq context → GFP___
5. Need zeroed memory → GFP_KERNEL | ___
6. Need DMA-capable memory → GFP___

WHY: GFP_KERNEL can sleep, GFP_ATOMIC cannot
```

---

## FAILURE PREDICTIONS

```
FAILURE 1: ceil(27000/4096) = 6.59 → need 7 pages, not 6
FAILURE 2: Buddy XOR calculation off by one bit → wrong buddy
FAILURE 3: Forgetting slab metadata → objects per slab too high
FAILURE 4: kmalloc(4097) needs order-1 via alloc_pages, not slab
FAILURE 5: GFP_KERNEL in interrupt context → deadlock
FAILURE 6: Internal fragmentation percentage = waste/allocated, not waste/requested
```

---

## W-QUESTIONS — NUMERICAL ANSWERS

### WHAT: Buddy Block
```
Order-3 block = 2^3 = 8 contiguous pages
8 pages × 4096 bytes = 32768 bytes = 32KB
PFN range: if starts at PFN 0x1000, contains PFN [0x1000, 0x1007]
Physical address range: 0x1000000 to 0x1007FFF (32KB)
```

### WHY: Power of 2
```
Split order-3 into two order-2:
32KB → 16KB + 16KB (even split)
If not power of 2 (say 24KB):
24KB → 12KB + 12KB? No standard order!
Powers of 2 allow binary splitting
2^10 = 1024, 2^11 = 2048, no 1536
```

### WHERE: Free Lists
```
Zone NORMAL at node 0:
  free_area[0]: 1234 free order-0 pages
  free_area[1]: 567 free order-1 blocks
  free_area[10]: 3 free order-10 blocks
Each block in linked list at zone->free_area[order].free_list
```

### WHO: Calls Allocator
```
alloc_pages(GFP_KERNEL, 2) → kernel module requests order-2
kmalloc(100, GFP_KERNEL) → slab layer requests order-0
do_anonymous_page() → page fault requests order-0
vmalloc(1MB) → requests 256 order-0 pages non-contiguous
```

### WHEN: Split Happens
```
Request order-2 (16KB)
free_area[2] empty, free_area[3] has 1 block
T₁: remove block from free_area[3]
T₂: split into two order-2 blocks
T₃: return first order-2, add second to free_area[2]
```

### WITHOUT: No Coalescing
```
Allocate 1000 order-0 pages, free all
Without coalesce: 1000 order-0 entries in free list
With coalesce: buddies merge → fewer larger blocks
Request order-5? Without coalesce: FAIL (no 32-page block)
With coalesce: SUCCEED (buddies merged to order-5)
```

### WHICH: Buddy Address
```
PFN = 0x1234, order = 2
Buddy = PFN XOR (1 << order) = 0x1234 XOR 0x4 = 0x1230
Check: 0x1230 XOR 0x4 = 0x1234 ✓ (mutual buddies)
0x1234 binary: 0001 0010 0011 0100
0x4 binary:    0000 0000 0000 0100
XOR:           0001 0010 0011 0000 = 0x1230
```

---

## ANNOYING CALCULATIONS — BREAKDOWN

### Annoying: Order from Size
```
Request 13000 bytes
Pages = ceil(13000 / 4096) = ceil(3.17) = 4 pages
Order = ceil(log2(4)) = 2
Actual = 2^2 × 4096 = 16384 bytes
Waste = 16384 - 13000 = 3384 bytes = 20.7%
```

### Annoying: Slab Math
```
Object size = 200 bytes
Page = 4096 bytes, metadata = 0 bytes (external)
Objects per slab = floor(4096 / 200) = floor(20.48) = 20
Used = 20 × 200 = 4000 bytes
Waste = 4096 - 4000 = 96 bytes = 2.3%
```

### Annoying: kmalloc Size Class
```
Request 65 bytes
Classes: 8, 16, 32, 64, 128, 256...
Smallest ≥ 65 is 128
Actual = 128 bytes
Waste = 128 - 65 = 63 bytes = 49.2%
```

### Annoying: Buddy Split Count
```
Have order-6 only, need order-2
Splits: 6→5, 5→4, 4→3, 3→2
Count = 6 - 2 = 4 splits
After: 1 order-2 (returned), 1 each of order-5,4,3,2 (in free lists)
```

---

## ATTACK PLAN

```
1. Size → pages: ceil(size / 4096)
2. Pages → order: ceil(log2(pages))
3. Order → actual: 2^order × 4096
4. Buddy: PFN XOR (1 << order)
5. Split count = have_order - need_order
```

---

## ADDITIONAL FAILURE PREDICTIONS

```
FAILURE 7: ceil(3.17) = 4, not 3 → undercounting pages
FAILURE 8: log2(5) = 2.32 → need order 3, not 2
FAILURE 9: Order-10 is MAX on most systems → 4MB max contiguous
FAILURE 10: GFP_KERNEL in interrupt → deadlock, use GFP_ATOMIC
```

---

## SHELL COMMANDS — PARADOXICAL THINKING EXERCISES

### COMMAND 1: View Buddy Allocator State

```bash
cat /proc/buddyinfo

# WHAT: Free blocks at each order, per zone, per node
# WHY: Shows fragmentation state of physical memory
# WHERE: zone->free_area[order].nr_free
# WHO: buddy allocator manages, /proc exposes
# WHEN: updated on every alloc/free
# WITHOUT: blind allocation, no visibility into fragmentation
# WHICH: order 0-10 (4KB to 4MB blocks)

# EXAMPLE OUTPUT:
# Node 0, zone  Normal  12851 8742 5211 2834 1203 412 127 38 11 2 142
#                        │     │    │    │    │   │   │  │  │  │  │
#                        │     │    │    │    │   │   │  │  │  │  └─ order10: 142×4MB = 568MB
#                        │     │    │    │    │   │   │  │  │  └─── order9: 2×2MB = 4MB
#                        │     │    │    │    │   │   │  │  └───── order8: 11×1MB = 11MB
#                        │     │    │    │    │   │   │  └─────── order7: 38×512KB = 19MB
#                        │     │    │    │    │   │   └───────── order6: 127×256KB = 31MB
#                        │     │    │    │    │   └──────────── order5: 412×128KB = 51MB
#                        │     │    │    │    └─────────────── order4: 1203×64KB = 75MB
#                        │     │    │    └──────────────────── order3: 2834×32KB = 88MB
#                        │     │    └───────────────────────── order2: 5211×16KB = 81MB
#                        │     └────────────────────────────── order1: 8742×8KB = 68MB
#                        └─────────────────────────────────── order0: 12851×4KB = 50MB
#
# TOTAL FREE = 50+68+81+88+75+51+31+19+11+4+568 = 1046MB

# CALCULATION:
# Can we allocate 1GB contiguous?
# Need order10 = 1GB = 262144 pages = 2^18... wait
# 2^10 pages = 1024 pages = 4MB, not 1GB!
# For 1GB need: 1GB/4KB = 262144 = 2^18 pages
# But max order = 10 = 1024 pages = 4MB
# ∴ CANNOT allocate 1GB contiguous via buddy
```

### COMMAND 2: View Slab Cache Info

```bash
cat /proc/slabinfo | head -20

# MEMORY DIAGRAM for task_struct slab:
# ┌────────────────────────────────────────────────────────────────────┐
# │ SLAB CACHE: "task_struct"                                          │
# │                                                                    │
# │ objsize = 4352 bytes (as shown in slabinfo)                        │
# │ objperslab = 7 objects                                             │
# │ pagesperslab = 8 pages = 32KB                                      │
# │                                                                    │
# │ CALCULATION:                                                       │
# │ 8 pages = 32768 bytes                                              │
# │ 7 objects × 4352 bytes = 30464 bytes                               │
# │ Overhead = 32768 - 30464 = 2304 bytes = 7% waste                   │
# │                                                                    │
# │ One slab layout:                                                   │
# │ ┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┬────┐ │
# │ │obj[0]   │obj[1]   │obj[2]   │obj[3]   │obj[4]   │obj[5]   │obj6│ │
# │ │4352B    │4352B    │4352B    │4352B    │4352B    │4352B    │4352│ │
# │ │@0x0     │@0x1100  │@0x2200  │@0x3300  │@0x4400  │@0x5500  │6600│ │
# │ └─────────┴─────────┴─────────┴─────────┴─────────┴─────────┴────┘ │
# │ Padding: 2304 bytes at end                                         │
# └────────────────────────────────────────────────────────────────────┘
```

### COMMAND 3: Force Fragmentation and Observe

```bash
cat << 'EOF' > /tmp/fragment.c
#include <stdlib.h>
#include <stdio.h>

int main() {
    void *ptrs[10000];
    // Allocate alternating
    for (int i = 0; i < 10000; i++)
        ptrs[i] = malloc(4096);
    // Free every other one
    for (int i = 0; i < 10000; i += 2)
        free(ptrs[i]);
    printf("Freed 5000 × 4KB = 20MB, but fragmented!\n");
    getchar();  // Pause to observe
}
EOF
gcc /tmp/fragment.c -o /tmp/fragment

# RUN: in one terminal run /tmp/fragment
# IN ANOTHER: watch cat /proc/buddyinfo
#
# CALCULATION:
# Before free: contiguous heap
# After free: 5000 × 4KB holes scattered
# Each hole = 1 page = order-0
# Cannot satisfy order-1 (8KB) request without compaction
#
# PARADOX: We freed 20MB but can't allocate 8KB contiguous?
# ANSWER: Buddy only merges if BOTH buddies are free
# Pattern: [FREE][USED][FREE][USED]... → no buddies adjacent
```

### COMMAND 4: kmalloc Size Class Test

```bash
# See kmalloc caches
cat /proc/slabinfo | grep kmalloc

# OUTPUT shows:
# kmalloc-8192, kmalloc-4096, kmalloc-2048, kmalloc-1024
# kmalloc-512, kmalloc-256, kmalloc-128, kmalloc-96
# kmalloc-64, kmalloc-32, kmalloc-16, kmalloc-8

# CALCULATION:
# Request 100 bytes → round up to 128 → kmalloc-128
# Waste = 128 - 100 = 28 bytes = 21.9%
#
# Request 1 byte → kmalloc-8
# Waste = 8 - 1 = 7 bytes = 87.5% waste!
#
# Request 4097 bytes → kmalloc-8192
# Waste = 8192 - 4097 = 4095 bytes = 49.97% waste!
#
# BEST CASE: request exactly power of 2
# kmalloc(512) → kmalloc-512 → 0% waste
```

---

## FINAL PARADOX QUESTIONS

```
Q1: 16GB RAM, but max contiguous alloc is 4MB?
    
    ANSWER:
    MAX_ORDER = 11 on most systems → 2^10 pages = 4MB max
    Even with 16GB free, buddy limits contiguous to 4MB
    For > 4MB contiguous: use CMA or hugetlbfs
    
Q2: Why does slab exist on top of buddy?
    
    CALCULATION:
    task_struct = 4352 bytes
    Buddy minimum = 4096 bytes → CANNOT fit!
    Would need order-1 = 8192 bytes per task_struct
    8192 / 4352 = 1.88 → 1 task per 2 pages → 53% waste
    
    With slab: 7 tasks per 8 pages = 32KB / 7 = 4681 per task
    4681 - 4352 = 329 bytes overhead = 7.5% waste
    
    Slab saves: 53% - 7.5% = 45.5% memory saved!
    
Q3: GFP_ATOMIC can fail. What happens?
    
    ANSWER:
    No free pages in any zone: return NULL
    Cannot reclaim (no sleeping allowed)
    Caller MUST handle NULL: if (!ptr) return -ENOMEM;
    Unlike GFP_KERNEL which retries and reclaims
```



## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: ORDER CALCULATION LOOP
START: SIZE=14000

C1. PAGES_NEEDED:
14000 / 4096 = 3 remainder 1712
3 + 1 = 4 pages
IDX_SET = {0, 1, 2, 3}
TOTAL_BYTES = 4 * 4096 = 16384

C2. ORDER_CALC:
O=0 → 2^0=1 < 4 ✗
O=1 → 2^1=2 < 4 ✗
O=2 → 2^2=4 >= 4 ✓
RESULT_ORDER = 2

C3. BUDDY_SEARCH (Zones):
ZONE_NORMAL(2) free_area[2].nr_free = 0 ✗
ZONE_NORMAL(2) free_area[3].nr_free = 1 ✓ (Split needed)

C4. SPLIT_OP:
BLOCK_BASE = 0x1000 (Ord3)
SPLIT 0x1000(Ord3) → 0x1000(Ord2) + 0x1004(Ord2)
ADD 0x1004 TO free_area[2]
RETURN 0x1000
STATE: free_area[3]-- → free_area[2]++

C5. ADDRESS_CHECK:
PFN=0x1000
PA=0x1000000
SIZE=16KB
RANGE=[0x1000000, 0x1004000)
14000 < 16384 ✓

C6. FREE_OP (MERGE):
FREE(0x1000, Ord2)
BUDDY = 0x1000 ^ (1<<2) = 0x1000 ^ 4 = 0x1004
IS_BUDDY_FREE(0x1004)? YES
REMOVE 0x1004 FROM free_area[2]
MERGE 0x1000 + 0x1004 → 0x1000(Ord3)
ADD 0x1000 TO free_area[3]
STATE: free_area[2]-- → free_area[3]++ ✓


---

[← Previous Lesson](../module_02_page_fault/lesson_02.md) | [Course Index](../index.md) | [Next Lesson →](../module_04_struct_page/lesson_04.md)
