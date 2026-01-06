---
title: "CR3 Register"
difficulty: Beginner
order: 1
---

# CR3 Register and PML4 Address

The CR3 register contains the physical address of the Page Map Level 4 (PML4) table.

## Key Facts

| Property | Value |
|----------|-------|
| CR3 Size | 64 bits |
| PML4 Address | bits [51:12] |
| PCID | bits [11:0] (if enabled) |
| Ring Required | 0 (kernel only) |

## Extract PML4 Address

```c
unsigned long cr3, pml4_phys;
asm volatile("mov %%cr3, %0" : "=r"(cr3));
pml4_phys = cr3 & 0x000FFFFFFFFFF000UL;
```

## Example Calculation

```
CR3 = 0x0000000305DEF005
Mask = 0x000FFFFFFFFFF000
PML4_phys = 0x0000000305DEF000
```

## Exercises

1. Calculate the PML4 physical address from CR3 = 0x80000001A2B3C010
2. What is the PCID value in the above CR3?
3. How many bytes is the PML4 table?
