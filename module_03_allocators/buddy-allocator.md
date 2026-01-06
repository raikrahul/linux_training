---
title: "Buddy Allocator"
difficulty: Intermediate
order: 10
---

# Buddy Allocator

The buddy allocator manages physical memory in power-of-2 sized blocks.

## Order to Size Mapping

| Order | Pages | Bytes | Human |
|-------|-------|-------|-------|
| 0 | 1 | 4,096 | 4 KB |
| 1 | 2 | 8,192 | 8 KB |
| 2 | 4 | 16,384 | 16 KB |
| 3 | 8 | 32,768 | 32 KB |
| 4 | 16 | 65,536 | 64 KB |
| 5 | 32 | 131,072 | 128 KB |
| 10 | 1024 | 4,194,304 | 4 MB |
| 11 | 2048 | 8,388,608 | 8 MB |

## Splitting Example

Request order-2 (4 pages), only order-4 available:

```
Order-4 (16 pages) at 0x100000
        ↓ split
┌───────────────────┐ ┌───────────────────┐
│ Order-3 (8 pages) │ │ Order-3 (8 pages) │
│ 0x100000          │ │ 0x108000          │
└───────────────────┘ └───────────────────┘
        ↓ split left
┌─────────┐ ┌─────────┐
│ Order-2 │ │ Order-2 │
│ 0x100000│ │ 0x104000│
└─────────┘ └─────────┘
    ↓
 RETURN    → free list
```

## Buddy Address Calculation

```c
buddy_addr = block_addr XOR (block_size_bytes)
// Example: 0x100000 XOR 0x4000 = 0x104000
```

## API

```c
struct page *alloc_pages(gfp_t gfp, unsigned int order);
void __free_pages(struct page *page, unsigned int order);
```

## Exercises

1. Calculate order needed for 100 KB allocation
2. Find buddy of 0x200000 at order 3
3. How much waste for 5 MB allocation?
