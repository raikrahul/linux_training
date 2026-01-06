# Investigation: Decoding the Struct Page Map

**Date**: 2025-12-30
**Subject**: Comprehensive Page State Decoding
**Machine**: x86_64

---

## 1. Axiomatic Foundation

### The Rosetta Stone
*   **AXIOM**: `struct page` contains all physical state.
*   **GOAL**: Given a `struct page *`, derive exactly what it represents.

## 2. Derivation: The Decision Tree

1.  **Check `_mapcount`**:
    *   -1? Page is not mapped into any user page tables.
    *   >=0? Page is mapped (User Space).

2.  **Check `mapping` (LSB)**:
    *   `mapping & 1`? **Anonymous** (Heap/Stack).
        *   `mapping & 2`? **KSM** (Kernel Samepage Merging) - specialized case.
    *   LSB 0? **File Backed**.

3.  **Check `flags`**:
    *   `PG_buddy` set? **Free** (in Allocator).
    *   `PG_slab` set? **Kernel Object** (Slab).
    *   `PG_reserved` set? **Hardware/Bios** reserved.

## 3. Experiment: The Decoder

*   [mapping_hw.c](https://github.com/raikrahul/linux_kernel_portfolio/blob/main/investigations/mapping_field_decode/mapping_hw.c): A unified decoder module.
*   **TESTS**:
    *   Allocate `malloc()` -> Decoder says "ANON, Mapped".
    *   Allocate `mmap(file)` -> Decoder says "FILE, Mapped".
    *   Free Page -> Decoder says "FREE (Buddy)".

## 4. Conclusion
We have derived the complete taxonomy of a physical page. By reading just 64 bytes of metadata, we can reconstruct the entire life story of a 4KB block of RAM.
