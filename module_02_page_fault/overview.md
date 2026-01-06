# Module 2: Page Fault Handling

## Overview

This module provides an in-depth exploration of the Linux page fault handling mechanism. You will trace the complete path from hardware exception to memory allocation, understanding every function in the chain.

## Learning Objectives

By the end of this module, you will be able to:

1. Explain how the CPU generates a page fault exception
2. Trace the call chain from `do_page_fault` to `handle_pte_fault`
3. Decode the error_code bits to understand fault type
4. Understand demand paging and copy-on-write mechanisms
5. Write kprobes to trace page fault handling

## Key Concepts

### Page Fault Entry Point

When the CPU cannot translate a virtual address, it:
1. Pushes error_code onto the stack
2. Stores faulting address in CR2
3. Jumps to the page fault handler (IDT entry 14)

```
Hardware Exception
       ↓
exc_page_fault()
       ↓
do_user_addr_fault()
       ↓
handle_mm_fault()
       ↓
handle_pte_fault()
       ↓
do_anonymous_page() / do_fault() / do_wp_page()
```

### Error Code

The error_code is a bitfield indicating fault cause:

| Bit | Name | Meaning when set |
|-----|------|------------------|
| 0 | P | Page was present (protection fault) |
| 1 | W | Write access caused fault |
| 2 | U | Fault occurred in user mode |
| 3 | RSVD | Reserved bit violation |
| 4 | I | Instruction fetch |

### Fault Types

1. **Demand Paging**: First access to anonymous memory
2. **Copy-on-Write**: Write to shared page after fork()
3. **File-backed Fault**: Page not in page cache
4. **Swap Fault**: Page was swapped out

## Hands-On Files

| File | Description |
|------|-------------|
| `fault_context_axiom.md` | CPU state during fault |
| `error_code_axioms.md` | Error code bit derivation |
| `handle_mm_fault_worksheet.md` | handle_mm_fault tracing |
| `handle_pte_fault_worksheet.md` | PTE-level handling |
| `do_anonymous_page_trace.md` | Anonymous page allocation |

## Prerequisites

- Module 1: Memory Fundamentals
- Understanding of page tables
- Basic C programming

## Next Module

[Module 3: Memory Allocators →](../module_03_allocators/)
