# Module 10: NUMA & Memory Zones

## Overview

This module covers Non-Uniform Memory Access (NUMA) and Linux memory zones. You will understand how the kernel optimizes memory allocation for performance.

---

## 1. NUMA Architecture

### What is NUMA?

In NUMA systems, memory access time depends on which CPU accesses which memory:

```
┌──────────────────────────────────────────────────────────────────┐
│                         NUMA System                               │
│                                                                   │
│   Node 0                              Node 1                      │
│   ┌─────────────┐                     ┌─────────────┐            │
│   │ CPU 0  CPU 1│                     │ CPU 4  CPU 5│            │
│   │ CPU 2  CPU 3│                     │ CPU 6  CPU 7│            │
│   └──────┬──────┘                     └──────┬──────┘            │
│          │                                   │                    │
│   ┌──────▼──────┐                     ┌──────▼──────┐            │
│   │  Local RAM  │                     │  Local RAM  │            │
│   │   32 GB     │                     │   32 GB     │            │
│   │ ~80ns access│                     │ ~80ns access│            │
│   └──────┬──────┘                     └──────┬──────┘            │
│          │                                   │                    │
│          └───────────────┬───────────────────┘                    │
│                          │                                        │
│                   Interconnect                                    │
│                 (QPI/UPI/Infinity Fabric)                        │
│                                                                   │
│   CPU 0 accessing Node 1 RAM: ~150ns (remote)                    │
│   CPU 0 accessing Node 0 RAM: ~80ns (local)                      │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

### NUMA Distance

```bash
$ numactl --hardware
available: 2 nodes (0-1)
node 0 cpus: 0 1 2 3
node 0 size: 32768 MB
node 1 cpus: 4 5 6 7
node 1 size: 32768 MB
node distances:
node   0   1
  0:  10  21
  1:  21  10
```

Distance 10 = local, 21 = remote (2.1x latency)

---

## 2. Memory Zones

### Zone Types

```c
// include/linux/mmzone.h
enum zone_type {
    ZONE_DMA,      // 0-16MB, ISA DMA
    ZONE_DMA32,    // 0-4GB, 32-bit DMA
    ZONE_NORMAL,   // 4GB+, regular use
    ZONE_MOVABLE,  // Movable pages (hotplug, migration)
    __MAX_NR_ZONES
};
```

### Zone Layout (64-bit system)

```
Physical Address Space:
┌─────────────────────────────────────────────────────────────────┐
│                                                                  │
│  0x00000000 ──► ┌─────────────────┐                             │
│                 │   ZONE_DMA      │  0 - 16MB                   │
│  0x01000000 ──► ├─────────────────┤                             │
│                 │                 │                              │
│                 │   ZONE_DMA32    │  16MB - 4GB                 │
│                 │                 │                              │
│  0x100000000 ─► ├─────────────────┤                             │
│                 │                 │                              │
│                 │   ZONE_NORMAL   │  4GB - end of RAM           │
│                 │                 │                              │
│  End of RAM  ─► └─────────────────┘                             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Per-Node Zones

```
Node 0:
  ZONE_DMA:     0-16MB
  ZONE_DMA32:   16MB-4GB
  ZONE_NORMAL:  4GB-32GB

Node 1:
  ZONE_NORMAL:  32GB-64GB (no DMA zones on node 1)
```

---

## 3. Viewing Zone Information

### /proc/zoneinfo

```bash
$ cat /proc/zoneinfo | head -50
Node 0, zone   Normal
  pages free     1234567
        min      12345
        low      15432
        high     18519
  node_scanned  0
  spanned  8388608
  present  8388608
  managed  8123456
        nr_free_pages 1234567
        nr_zone_inactive_anon 12345
        nr_zone_active_anon 23456
        nr_zone_inactive_file 34567
        nr_zone_active_file 45678
```

### /proc/buddyinfo

```bash
$ cat /proc/buddyinfo
Node 0, zone    DMA      1      0      0      1      2      1      1      0      1      1      3
Node 0, zone  DMA32   3912   3015   2107   1293    624    243     82     24      8      3     67
Node 0, zone Normal  12851   8742   5211   2834   1203    412    127     38     11      2    142
Node 1, zone Normal  15234   9876   6543   3210   1234    567    234    123     45     12    189
```

---

## 4. Zone Fallback

### Fallback Order

When preferred zone is exhausted:

```
Request for ZONE_NORMAL:
  1. Try ZONE_NORMAL on local node
  2. Try ZONE_NORMAL on remote nodes
  3. Try ZONE_DMA32 on local node
  4. Try ZONE_DMA32 on remote nodes
  5. Try ZONE_DMA (last resort)
  6. Fail with -ENOMEM
```

### Code

```c
// mm/page_alloc.c (simplified)
static struct page *get_page_from_freelist(gfp_t gfp_mask,
                                           unsigned int order,
                                           int alloc_flags,
                                           struct zonelist *zonelist)
{
    struct zoneref *z;
    struct zone *zone;
    
    // Walk zonelist in priority order
    for_each_zone_zonelist_nodemask(zone, z, zonelist, gfp_zone(gfp_mask)) {
        
        // Skip if zone is too low on memory
        if (!zone_watermark_ok(zone, order, mark))
            continue;
        
        page = rmqueue(zone, order, gfp_mask);
        if (page)
            return page;
    }
    
    return NULL;  // All zones exhausted
}
```

---

## 5. NUMA Memory Policy

### Policy Types

```c
// include/uapi/linux/mempolicy.h
#define MPOL_DEFAULT     0   // Use system default
#define MPOL_PREFERRED   1   // Prefer specific node
#define MPOL_BIND        2   // Restrict to nodes
#define MPOL_INTERLEAVE  3   // Round-robin across nodes
#define MPOL_LOCAL       4   // Allocate on local node
```

### User API

```c
#include <numaif.h>

int main() {
    // Force local allocation
    set_mempolicy(MPOL_LOCAL, NULL, 0);
    
    // Interleave across all nodes
    unsigned long nodemask = 0x3;  // Nodes 0 and 1
    set_mempolicy(MPOL_INTERLEAVE, &nodemask, 2);
    
    // Bind to specific node
    set_mempolicy(MPOL_BIND, &nodemask, 2);
}
```

### numactl

```bash
# Run on node 0
$ numactl --cpunodebind=0 --membind=0 ./my_program

# Interleave memory across nodes
$ numactl --interleave=all ./my_program

# Show NUMA statistics
$ numastat
                           node0           node1
numa_hit               123456789        987654321
numa_miss                   1234             5678
local_node             123456789        987654321
other_node                  1234             5678
```

---

## 6. Kernel Module: Zone Stats

```c
// zone_stats.c
#include <linux/module.h>
#include <linux/mmzone.h>
#include <linux/mm.h>

static void print_zone_stats(void)
{
    pg_data_t *pgdat;
    struct zone *zone;
    int nid;
    
    for_each_online_node(nid) {
        pgdat = NODE_DATA(nid);
        
        pr_info("Node %d:\n", nid);
        
        for_each_zone(zone) {
            if (!populated_zone(zone))
                continue;
            
            pr_info("  Zone %s:\n", zone->name);
            pr_info("    Free pages:  %lu\n",
                    zone_page_state(zone, NR_FREE_PAGES));
            pr_info("    Min:         %lu\n", zone->_watermark[WMARK_MIN]);
            pr_info("    Low:         %lu\n", zone->_watermark[WMARK_LOW]);
            pr_info("    High:        %lu\n", zone->_watermark[WMARK_HIGH]);
            pr_info("    Managed:     %lu\n", zone_managed_pages(zone));
        }
    }
}

static int __init zone_stats_init(void)
{
    print_zone_stats();
    return 0;
}

static void __exit zone_stats_exit(void) {}

module_init(zone_stats_init);
module_exit(zone_stats_exit);
MODULE_LICENSE("GPL");
```

---

## 7. Watermarks

### What are Watermarks?

```
Zone Free Pages:
     │
     │  ████████████████████████████████████████  ← Lots of free
     │  ████████████████████████████████████████
     │  ████████████████████████████████████████
HIGH │─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ── kswapd stops reclaiming
     │  ████████████████████████
     │  ████████████████████████
LOW  │─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ── kswapd starts reclaiming
     │  ████████████████
     │  ████████████████
MIN  │─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ── Direct reclaim required
     │  █████
     │  █████  ← Emergency, may OOM
     │
     └─────────────────────────────────────────────
```

---

## 8. Practice Exercises

### Exercise 1: NUMA Benchmark

Compare memory bandwidth for:
- Local access (CPU 0 → Node 0 RAM)
- Remote access (CPU 0 → Node 1 RAM)

### Exercise 2: Zone Exhaustion

Write a program that allocates until a specific zone is exhausted.
Monitor /proc/buddyinfo during allocation.

### Exercise 3: Policy Impact

Measure application performance with different NUMA policies:
- MPOL_LOCAL vs MPOL_INTERLEAVE
- Track numa_miss counter

---

## Course Complete!

Congratulations on completing the Linux Kernel Training course.

[← Back to Course Index](../index.md)

---

## AXIOMATIC EXERCISES — BRUTE FORCE CALCULATION

### EXERCISE A: NUMA DISTANCE CALCULATION

```
GIVEN:
  Local access latency = 80ns
  Distance matrix:
    node 0 to node 0: 10
    node 0 to node 1: 21
    node 1 to node 1: 10

TASK:

1. Distance 10 = local, baseline
2. Node 0 → Node 1 distance = 21 = 2.1 × baseline
3. Remote latency = 80ns × 2.1 = ___ ns
4. Extra latency per remote access = ___ - 80 = ___ ns
5. For 1 million remote accesses: extra time = ___ × 1000000 = ___ ms
```

### EXERCISE B: ZONE BOUNDARY CALCULATION

```
GIVEN: System with 64GB RAM

TASK: Calculate zone boundaries

1. ZONE_DMA: 0 - 16MB = 0 - 0x___ = ___ pages
2. ZONE_DMA32: 16MB - 4GB = 0x___ - 0x___ = ___ pages
3. ZONE_NORMAL: 4GB - 64GB = 0x___ - 0x___ = ___ pages
4. Total pages = ___ + ___ + ___ = ___
5. Verify: 64GB / 4KB = ___ pages ✓
```

### EXERCISE C: WATERMARK THRESHOLDS

```
GIVEN zone:
  managed_pages = 1000000
  min = 1% of managed = ___
  low = min × 1.25 = ___
  high = min × 1.5 = ___

TASK:

1. min watermark = 1000000 × 0.01 = ___ pages
2. low watermark = ___ × 1.25 = ___ pages
3. high watermark = ___ × 1.5 = ___ pages

CURRENT: free_pages = 12000

4. free_pages > high? ___ > ___ → kswapd sleeping? ___
5. free_pages < low? ___ < ___ → kswapd wakes? ___
6. free_pages < min? ___ < ___ → direct reclaim? ___
```

### EXERCISE D: NUMA POLICY EFFECT

```
GIVEN:
  Application runs on CPU 3 (Node 0)
  Node 0: 20GB free
  Node 1: 30GB free

TASK: Where does memory come from?

With MPOL_LOCAL:
1. Prefer node = current CPU's node = ___
2. Allocate from node ___ first
3. Fall back to node ___ if node 0 exhausted

With MPOL_INTERLEAVE:
1. Allocation 0 → node ___
2. Allocation 1 → node ___
3. Allocation 2 → node ___
4. Pattern: round-robin

With MPOL_BIND to node 1:
1. All allocations → node ___
2. Node 0 has 20GB free but → NOT USED
3. If node 1 exhausted → OOM (not fallback!)
```

---

## FAILURE PREDICTIONS

```
FAILURE 1: Distance 21 means 2.1× latency, not 21× → calculation error
FAILURE 2: ZONE_DMA32 is 0-4GB, not 16MB-4GB only → includes DMA
FAILURE 3: Watermarks are in PAGES not bytes → unit confusion
FAILURE 4: MPOL_BIND does NOT fall back → can OOM with free memory elsewhere
FAILURE 5: kswapd wakes at LOW, not MIN → direct reclaim only at MIN
FAILURE 6: ZONE_NORMAL on 32-bit is DIFFERENT from 64-bit → config dependent
```
