# Linux Kernel Training

Professional deep-dive training into Linux kernel internals. Hands-on modules with real machine traces.

---

## Course Modules

| # | Module | Lesson | Topics |
|---|--------|--------|--------|
| 1 | Memory Fundamentals | [Lesson 01](module_01_memory_fundamentals/lesson_01.md) | Page tables, CR3, TLB, PCID, VMA |
| 2 | Page Fault Handling | [Lesson 02](module_02_page_fault/lesson_02.md) | do_page_fault, handle_mm_fault, error codes |
| 3 | Memory Allocators | [Lesson 03](module_03_allocators/lesson_03.md) | Buddy allocator, Slab, GFP flags |
| 4 | struct page Deep Dive | [Lesson 04](module_04_struct_page/lesson_04.md) | Page flags, refcounting, mapping field |
| 5 | Advanced Memory | [Lesson 05](module_05_advanced_memory/lesson_05.md) | LRU, mlock, anonymous pages, page cache |
| 6 | Kprobe Tracing | [Lesson 06](module_06_kprobe_tracing/lesson_06.md) | register_kprobe, pt_regs, handler writing |
| 7 | Network Stack Tracing | [Lesson 07](module_07_network_tracing/lesson_07.md) | sk_buff, copy_from_iter, double copy |
| 8 | RDMA Fundamentals | [Lesson 08](module_08_rdma/lesson_08.md) | ibv_reg_mr, zero-copy, queue pairs |
| 9 | Maple Tree & VMA | [Lesson 09](module_09_maple_tree/lesson_09.md) | VMA lookup, address space, maple tree |
| 10 | NUMA & Zones | [Lesson 10](module_10_numa_zones/lesson_10.md) | NUMA nodes, memory zones, fallback |

---

## What Each Lesson Contains

```
┌─────────────────────────────────────────────────────────────────────┐
│ LESSON STRUCTURE                                                    │
│                                                                     │
│ 1. Core Concepts with ASCII Diagrams                                │
│ 2. Kernel Code Examples (with file:line references)                 │
│ 3. Userspace Examples (compilable C programs)                       │
│ 4. AXIOMATIC EXERCISES — fill-in-the-blank calculations             │
│ 5. W-QUESTIONS — What/Why/Where/Who/When/Without/Which              │
│ 6. ANNOYING CALCULATIONS — step-by-step numerical breakdowns        │
│ 7. SHELL COMMANDS — paradoxical thinking with real commands         │
│ 8. FAILURE PREDICTIONS — common mistakes and how to avoid           │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Prerequisites

- Linux x86_64 (kernel 6.x recommended)
- GCC, Make
- Root access for kernel modules
- Basic C programming

---

## Methodology

Every concept is:
1. **Derived** from kernel source (no hand-waving)
2. **Traced** with real machine data (dmesg, /proc)
3. **Verified** with kprobes and live proofs
4. **Calculated** step-by-step (primate-friendly)

---

## Target Audience

- Systems programmers wanting kernel internals
- Performance engineers debugging memory issues
- Security researchers analyzing kernel behavior
- Anyone tired of "magic happens here" explanations

---

## Quick Start

```bash
# Clone the repository
git clone https://github.com/raikrahul/linux_training.git
cd linux_training

# Start with Lesson 01
cat module_01_memory_fundamentals/lesson_01.md
```

---

Built on Linux 6.14+ x86_64. All data from real machine traces.
