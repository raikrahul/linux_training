# Linux Kernel Training

Professional deep-dive training into Linux kernel internals. Hands-on modules with real machine traces.

## Course Modules

| # | Module | Topics |
|---|--------|--------|
| 1 | [Memory Fundamentals](module_01_memory_fundamentals/) | Page tables, CR3, TLB, PCID, VMA |
| 2 | [Page Fault Handling](module_02_page_fault/) | do_page_fault, handle_mm_fault, error codes |
| 3 | [Memory Allocators](module_03_allocators/) | Buddy allocator, Slab, GFP flags |
| 4 | [struct page Deep Dive](module_04_struct_page/) | Page flags, refcounting, mapping field |
| 5 | [Advanced Memory](module_05_advanced_memory/) | LRU, mlock, anonymous pages, page cache |
| 6 | [Kprobe Tracing](module_06_kprobe_tracing/) | register_kprobe, pt_regs, handler writing |
| 7 | [Network Stack Tracing](module_07_network_tracing/) | sk_buff, copy_from_iter, double copy |
| 8 | [RDMA Fundamentals](module_08_rdma/) | ibv_reg_mr, zero-copy, queue pairs |
| 9 | [Maple Tree & VMA](module_09_maple_tree/) | VMA lookup, address space, maple tree |
| 10 | [NUMA & Zones](module_10_numa_zones/) | NUMA nodes, memory zones, fallback |

## Prerequisites

- Linux x86_64 (kernel 6.x recommended)
- GCC, Make
- Root access for kernel modules
- Basic C programming

## Methodology

Every concept is:
1. **Derived** from kernel source (no hand-waving)
2. **Traced** with real machine data (dmesg, /proc)
3. **Verified** with kprobes and live proofs
4. **Calculated** step-by-step (primate-friendly)

## Target Audience

- Systems programmers wanting kernel internals
- Performance engineers debugging memory issues
- Security researchers analyzing kernel behavior
- Anyone tired of "magic happens here" explanations

## License

Training content for professional development.

---

Built on Linux 6.14+ x86_64. All data from real machine traces.
