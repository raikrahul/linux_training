# Investigation: The Struct Page Union

**Date**: 2025-12-29
**Subject**: C Unions and `struct page` Efficiency
**Machine**: x86_64

---

## 1. Axiomatic Foundation

### The Context Switch
*   **AXIOM**: A page cannot be everything at once.
*   **STATES**:
    1.  **Free** (Buddy System): Needs `order`.
    2.  **Page Cache** (File): Needs `index`.
    3.  **Anonymous** (Heap): Needs `anon_vma`.
    4.  **Slab** (Kernel Objects): Needs `s_mem`, `freelist`.

### The Optimization
*   **PROBLEM**: If we store fields for ALL states, `struct page` grows to 200 bytes.
*   **SOLUTION**: The C `union`. Overlap fields that are mutually exclusive.

## 2. Derivation: Decoding the Union

```c
struct page {
    unsigned long flags;
    union {
        struct {
            // State: Page Cache
            struct list_head lru;
            struct address_space *mapping;
            pgoff_t index;
            unsigned long private;
        };
        struct {
            // State: Buddy (Free)
            unsigned long _buddy_combined_order_id;
            struct list_head buddy_list;
        };
        struct {
            // State: Slab
            unsigned long _s_mem;
            struct list_head slab_list;
        };
    };
};
```

*   **OFFSET 16**: Can be `mapping` OR `s_mem` OR `buddy_order`.
*   **OFFSET 40**: Can be `lru` OR `buddy_list` OR `slab_list`.

## 3. Experiment: Reading the Same Bytes

*   [metadata_trace.c](https://github.com/raikrahul/linux_kernel_portfolio/blob/main/investigations/metadata_union/metadata_union.c): Module to inspect a page in different states.
*   **OBSERVATION**:
    *   **Allocated Page**: Offset 16 is a pointer (`0xffff...`).
    *   **Freed Page**: Offset 16 becomes distinct integer patterns (Buddy Order).

## 4. Conclusion
The `struct page` is a shapeshifter. The meaning of its bytes depends entirely on its context (State). Reading `page->mapping` on a free page is not just wrong; it reads garbage data from the Buddy Allocator.
