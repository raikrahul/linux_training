---
title: "Page Walks: 4KB, 2MB, and 1GB"
difficulty: Intermediate
order: 3
---

# Page Walks

Comparison of 4KB, 2MB, and 1GB page walks.

## Walk Depth Comparison

| Page Size | Levels | RAM Reads | Offset Bits |
|-----------|--------|-----------|-------------|
| 4 KB | 4 | 4 | 12 |
| 2 MB | 3 | 3 | 21 |
| 1 GB | 2 | 2 | 30 |

## 4KB Walk (Full 4 levels)

```
CR3 → PML4[idx] → PDPT[idx] → PD[idx] → PT[idx] → 4KB Page
      P=1,PS=0    P=1,PS=0    P=1,PS=0   P=1
```

## 2MB Walk (Stop at PD)

```
CR3 → PML4[idx] → PDPT[idx] → PD[idx] → 2MB Page
      P=1,PS=0    P=1,PS=0    P=1,PS=1
                              ↑ PS=1 means huge page
```

## 1GB Walk (Stop at PDPT)

```
CR3 → PML4[idx] → PDPT[idx] → 1GB Page
      P=1,PS=0    P=1,PS=1
                  ↑ PS=1 means 1GB page
```

## Address Masks

```c
#define MASK_4KB  0x000FFFFFFFFFF000UL  // bits [51:12]
#define MASK_2MB  0x000FFFFFFFE00000UL  // bits [51:21]
#define MASK_1GB  0x000FFFFFC0000000UL  // bits [51:30]
```

## Time Savings

```
4KB: 4 reads × 100ns = 400ns (cold)
2MB: 3 reads × 100ns = 300ns (25% faster)
1GB: 2 reads × 100ns = 200ns (50% faster)
```
