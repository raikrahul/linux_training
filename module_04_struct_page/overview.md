# Module 4: struct page Deep Dive

## Overview

This module explores the `struct page` structure, the kernel's metadata for every physical page. You will learn how the kernel packs multiple use-cases into overlapping union fields.

## Learning Objectives

By the end of this module, you will be able to:

1. Locate struct page for any physical page frame
2. Decode page flags and their meanings
3. Understand the mapping field's dual role
4. Track page refcounts correctly
5. Explain compound page organization

## Key Concepts

### Page Frame to struct page

Every physical page frame has a corresponding `struct page`:

```
PFN (Page Frame Number) = physical_address / 4096
struct page *page = pfn_to_page(PFN)
struct page *page = virt_to_page(virtual_address)
```

### Page Flags

Flags are stored in page->flags with zone and node information:

```
flags layout (64 bits):
┌──────────────┬──────────────┬────────────────────────┐
│ Section (0)  │ Node + Zone  │ Actual page flags      │
└──────────────┴──────────────┴────────────────────────┘
```

Key flags:
- `PG_locked`: Page is locked for I/O
- `PG_dirty`: Page has been modified
- `PG_lru`: Page is on LRU list
- `PG_slab`: Page is used by slab allocator

### Mapping Field

The `mapping` field serves different purposes:

| Condition | Meaning |
|-----------|---------|
| NULL | Anonymous page, not mapped |
| LSB = 0 | File-backed, points to address_space |
| LSB = 1 | Anonymous, points to anon_vma |
| LSB = 2 | KSM page |

### Refcounting

Pages have two refcounts:
- `_refcount`: Page usage count
- `_mapcount`: Number of PTEs referencing this page

## Hands-On Files

| File | Description |
|------|-------------|
| `flags_worksheet.md` | Flag extraction and interpretation |
| `refcount_worksheet.md` | Refcount mechanics |
| `compound_worksheet.md` | Huge page organization |
| `struct_page_deep_dive.md` | Complete field analysis |

## Prerequisites

- Module 1: Memory Fundamentals
- Module 3: Memory Allocators
- Bit manipulation skills

## Next Module

[Module 5: Advanced Memory →](../module_05_advanced_memory/)
