# Linux Kernel Training

> **[View the Course](https://raikrahul.github.io/linux_training)**

Professional training modules for Linux kernel internals. Real machine traces. Zero hand-waving.

## What You'll Learn

- How page tables translate virtual to physical addresses
- What happens inside `do_page_fault()` and `handle_mm_fault()`
- How buddy allocator and slab allocator work at the bit level
- What every field in `struct page` means
- How to write kprobes to trace kernel functions
- How network data copies work (and why RDMA eliminates them)
- How NUMA and memory zones affect performance

## Modules

1. **Memory Fundamentals** — page tables, CR3, TLB, VMA
2. **Page Fault Handling** — fault path, error codes, PTE manipulation
3. **Memory Allocators** — buddy, slab, GFP flags
4. **struct page Deep Dive** — flags, refcount, mapping
5. **Advanced Memory** — LRU, mlock, page cache
6. **Kprobe Tracing** — runtime kernel instrumentation
7. **Network Tracing** — sk_buff, copy chain, double copy
8. **RDMA** — zero-copy networking
9. **Maple Tree** — VMA lookup internals
10. **NUMA & Zones** — memory topology

## Requirements

- Linux x86_64 kernel 6.x
- GCC, Make
- Root access

## Author

Systems Software Engineer focused on Linux kernel performance and debugging.

---

*All content derived from kernel source with real machine verification.*
