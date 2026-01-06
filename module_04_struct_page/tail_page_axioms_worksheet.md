# Axiomatic Derivation: Tail Page Metadata

## 01. Axiom: The Struct Page Layout
1.  **Axiom**: `struct page` size is 64 bytes (optimized for cache lines).
    - Source: `sizeof(struct page)` on x86_64.
    - Check: `grep` definition or run C code to print `sizeof`.
    - Math: 64 = 0x40.
2.  **Axiom**: `unsigned long` is 8 bytes. `unions` overlap at offset 0.
3.  **Layout Map** (from `mm_types.h` read):
    - `0x00`: `flags` (8 bytes).
    - `0x08`: **UNION START**:
        - Option A: `struct list_head lru` (16 bytes) -> `next` (8) + `prev` (8).
        - Option B: `unsigned long compound_head` (8 bytes).
    - `0x18`: **UNION CONTINUED** (or separate field?):
        - `mapping` is at offset `0x18` (24).
    - **Wait**: `lru` is at 0x08. Size 16. Ends at 0x08 + 0x10 = 0x18.
    - So `mapping` starts at 0x18.
    - **Critique**: Does `compound_head` overlap `lru.next`?
        - `lru` starts at 0x08 in `struct page`?
        - `mm_types.h`: `struct page` -> `unsigned long flags` (0x00) -> `union { ... }` (0x08).
        - Inside union: `struct { union { struct list_head lru; ... }; struct address_space *mapping; ... }` (Page Cache).
        - `lru` is at offset 0 *of the union*. Union is at offset 8 *of the struct*.
        - So `lru` is at 0x08. `compound_head` is at offset 0 *of the union*.
        - ∴ `compound_head` **overlays** `lru.next`.
        - `lru.prev` is at 0x10.
        - `mapping` is at 0x18.
    - **Conclusion**: `lru.next` and `compound_head` share the same 8 bytes at offset 0x08.

## 02. Axiom: Bit 0 Encoding (Compound Head)
1.  **Problem**: How to distinguish `lru.next` (valid pointer) and `compound_head` (pointer to head)?
2.  **Axiom**: Pointers to `struct page` are aligned.
    - `sizeof(struct page)` = 64 (0x40). 64 is a multiple of 2.
    - Also `kmalloc` alignment is usually 8 bytes.
    - ∴ Bottom bits [2:0] are always 0 for a valid pointer.
3.  **Mechanism**: `compound_head = (unsigned long)head_page | 1`.
4.  **Bit 0**: The "Tail Bit".
    - If `lru.next` has Bit 0 set -> IT IS NOT A LIST POINTER. It is a `compound_head`.
    - If `lru.next` has Bit 0 clear -> It is a valid `lru` pointer.
5.  **Calculations**:
    - **Input**: `HeadAddr` = 0xffffEA0000000000.
    - **Encode**: `0xffffEA0000000000 | 1` = `0xffffEA0000000001`.
    - **Storage**: `TailPage->compound_head` (Offset 0x08) = `0xffffEA0000000001`.
    - **Decode**: `val - 1` = `0xffffEA0000000000`. matches `HeadAddr`. ✓.

## 03. Axiom: TAIL_MAPPING Poison
1.  **Axiom Source**: `include/linux/poison.h`.
    - `#define TAIL_MAPPING ((void *) 0x400 + POISON_POINTER_DELTA)`
    - `POISON_POINTER_DELTA` = 0 (x86_64 default).
    - ∴ `TAIL_MAPPING` = `0x400` = 1024.
2.  **Location**:
    - `mapping` field is at offset 0x18 (24 bytes).
    - In `prep_compound_tail()`: `p->mapping = TAIL_MAPPING;`.
3.  **Why?**:
    - Accessing `page->mapping` on a tail page is a bug.
    - If code tries to dereference `0x400`, it faults (Near NULL).
    - Identify logic: IF `val == 0x400` THEN it is a Tail Page (Debugging check).

## 04. Divergence: The Struct Overlay (Listing 2-4 vs 6.14)
1.  **Observation**: `compound_dtor` (Destructor) not found in 6.14 headers (grep failed).
2.  **Reason**: modern kernels (Folder/THP refactor) removed `compound_dtor`.
    - `struct folio` handles dtors via `ops`.
3.  **Conflict**: Listing 2-4 shows `compound_dtor` in "First Tail Page".
    - Modern 6.14: "First Tail Page" might store `deferred_list` or `_folio_nr_pages`.
    - `mm_types.h`: `unsigned int _folio_nr_pages` is in the union.
4.  **Conclusion**: We will focus on `compound_head` and `mapping` (TAIL_MAPPING) which remain consistent. We will *not* try to decode `compound_dtor`.

## 05. Numerical Simulation (Memory Trace)
1.  **Scenario**: Alloc 1 Huge Page (2MB = 512 pages).
2.  **Head (PFN 0)**:
    - Addr: `0xffffEA0000000000`.
    - `flags`: `PageHead` set.
    - `mapping`: 0x1234 (Valid Anon VMA).
3.  **Tail 1 (PFN 1)**:
    - Addr: `0xffffEA0000000040` (+64 bytes).
    - Offset 0x08 (`compound_head`): `0xffffEA0000000001` (Head|1).
    - Offset 0x18 (`mapping`): `0x400` (TAIL_MAPPING).
4.  **Tail 2 (PFN 2)**:
    - Addr: `0xffffEA0000000080` (+128 bytes).
    - Offset 0x08 (`compound_head`): `0xffffEA0000000001`.
    - Offset 0x18 (`mapping`): `0x400`.
5.  **Checks**:
    - `PageTail(p)`: `p->compound_head & 1` ? Yes.
    - `compound_head(p)`: `p->compound_head - 1`.

## 06. ERROR REPORT: USER CONFUSIONS

### E01: NESTED UNION NOT UNDERSTOOD
- FILE: `/usr/src/linux-headers-6.14.0-37-generic/include/linux/mm_types.h`
- STRUCTURE: `struct page` has OUTER UNION at offset 0x08.
- INSIDE OPTION A: another INNER UNION at offset 0x08.
- USER MISSED: inner union contains `lru` OR `__filler+mlock_count` OR `buddy_list` OR `pcp_list`.
- CONSEQUENCE: user thought `lru` is fixed. It is NOT. It can be `buddy_list` when page is free.

### E02: __filler COMMENT NOT READ
- SOURCE LINE: `/* Always even, to negate PageTail */`
- MEANING: when using `__filler` (mlock path), bit 0 kept 0.
- WHY: to avoid false positive PageTail detection.
- USER MISSED: kernel developers already thought about bit collision.

### E03: TAIL_MAPPING MISMATCH IGNORED
- LIVE DATA: `mapping = 0x800000` (not 0x400).
- EXPECTED: `TAIL_MAPPING = 0x400`.
- USER DID NOT: investigate why 0x800000 appeared.
- POSSIBLE CAUSE: folio rework in 6.14 uses mapping for other metadata.
- ACTION NEEDED: grep for `0x800000` or `_folio` in mm_types.h.

### E04: UNIT CONFUSION (4096 vs 2048)
- USER THOUGHT: 4096 > 2048 so "4KB page bigger than huge page?"
- ACTUAL: 4096 bytes vs 2048 KB.
- ROOT: did not check UNITS. bytes vs kilobytes.
- FIX: always write unit suffix (4096 B, 2048 KB).

### E05: ALIGNMENT FORMULA NOT DERIVED
- FORMULA: `aligned = (addr + 0x1FFFFF) & ~0x1FFFFF`
- USER ASKED: "why this works?"
- USER DID NOT: trace binary operations step by step FIRST.
- CORRECT: derive `~0x1FFFFF` = mask, then trace AND operation.

### E06: struct page SIZE AXIOM ASSUMED
- USER ASKED: "I can just make struct page* anywhere".
- WRONG: struct page pointers come FROM vmemmap array only.
- USER DID NOT: understand vmemmap is kernel-allocated at boot.
- CONSEQUENCE: thought alignment was optional.

### E07: thp_fault_alloc NOT CONNECTED TO OBSERVATION
- LIVE DATA: thp_fault_alloc = 4 then 6 (before/after test).
- USER DID NOT: correlate this with AnonHugePages change.
- SHOULD HAVE: computed 6-4=2 new allocations = 2x2MB = 4MB allocated.

### E08: INNER UNION OPTIONS NOT ENUMERATED
- OPTIONS inside OPTION A at offset 0x08:
  1. `struct list_head lru` (active/inactive LRU list)
  2. `void *__filler + unsigned int mlock_count` (mlock path)
  3. `struct list_head buddy_list` (free page in buddy)
  4. `struct list_head pcp_list` (per-cpu page list)
- USER DID NOT: realize page uses DIFFERENT field depending on state.

### E09: DID NOT READ WARNING COMMENT
- SOURCE: `/* WARNING: bit 0 of the first word is used for PageTail() */`
- LOCATION: line 5-10 of union block in mm_types.h.
- USER SKIPPED: kernel explicitly warns about bit 0.

### E10: ASSUMED STRUCT IS SIMPLE
- REALITY: struct page is the most complex struct in Linux kernel.
- REASON: 4 million instances (16GB RAM) x 64 bytes = 256MB.
- EVERY BYTE MATTERS: hence aggressive union overlaying.
- USER EXPECTED: simple flat struct.

## 07. MUTUAL EXCLUSION DIAGRAM

### OUTER UNION at offset 0x08 (40 bytes total):

```
struct page {
    flags (0x00, 8B)
    |
    +-- UNION @ 0x08 ─────────────────────────────────────────────────────────┐
    |                                                                          |
    |   ┌─────────────────────────────────────────────────────────────────┐   |
    |   │ OPTION A: Page Cache / Anonymous                                │   |
    |   │   ┌─────────────────────────────────────────────────────────┐   │   |
    |   │   │ INNER UNION @ 0x08:                                     │   │   |
    |   │   │   ├── lru (list_head, 16B)   ← active/inactive LRU      │   │   |
    |   │   │   ├── __filler + mlock_count ← mlocked pages            │   │   |
    |   │   │   ├── buddy_list (list_head) ← FREE in buddy allocator  │   │   |
    |   │   │   └── pcp_list (list_head)   ← FREE in per-cpu cache    │   │   |
    |   │   └─────────────────────────────────────────────────────────┘   │   |
    |   │   mapping @ 0x18 (8B) → address_space or anon_vma              │   |
    |   │   index   @ 0x20 (8B) → offset in file                         │   |
    |   │   private @ 0x28 (8B) → buffer_head or swap entry              │   |
    |   └─────────────────────────────────────────────────────────────────┘   |
    |                                                                          |
    |   ┌─────────────────────────────────────────────────────────────────┐   |
    |   │ OPTION B: Tail pages of compound page                           │   |
    |   │   compound_head @ 0x08 (8B) ← HEAD_PTR | 1                      │   |
    |   │   (rest unused or TAIL_MAPPING in mapping @ 0x18)               │   |
    |   └─────────────────────────────────────────────────────────────────┘   |
    |                                                                          |
    |   ┌─────────────────────────────────────────────────────────────────┐   |
    |   │ OPTION C: page_pool (networking)                                │   |
    |   │   pp_magic, pp, dma_addr, pp_ref_count                          │   |
    |   └─────────────────────────────────────────────────────────────────┘   |
    |                                                                          |
    |   ┌─────────────────────────────────────────────────────────────────┐   |
    |   │ OPTION D: ZONE_DEVICE                                           │   |
    |   │   pgmap, zone_device_data                                       │   |
    |   └─────────────────────────────────────────────────────────────────┘   |
    |                                                                          |
    |   ┌─────────────────────────────────────────────────────────────────┐   |
    |   │ OPTION E: rcu_head (for RCU freeing)                            │   |
    |   └─────────────────────────────────────────────────────────────────┘   |
    |                                                                          |
    └──────────────────────────────────────────────────────────────────────────┘
}
```

### STATE → OPTION MAPPING:

```
+---------------------------+----------+------------------------+----------+
| Page State                | Option   | Field @ 0x08           | Bit 0    |
+---------------------------+----------+------------------------+----------+
| On active/inactive LRU    | A        | lru.next               | 0        |
| Mlocked (unevictable)     | A        | __filler               | 0 (even) |
| Free in buddy allocator   | A        | buddy_list.next        | 0        |
| Free in per-cpu cache     | A        | pcp_list.next          | 0        |
| TAIL of compound page     | B        | compound_head          | 1        |
| Network page_pool         | C        | pp_magic               | varies   |
| Device memory             | D        | pgmap                  | 0        |
| Being freed via RCU       | E        | rcu_head.next          | 0        |
+---------------------------+----------+------------------------+----------+
```

### RULE:
- Only ONE option is valid at any time.
- Bit 0 of offset 0x08 distinguishes TAIL (bit=1) from all others (bit=0).
- `__filler` comment: "Always even" → kernel forces bit 0 = 0 for mlock path.

## 08. ADDITIONAL ERROR REPORT (SESSION 2)

### E11: DID NOT KNOW WHAT `& 1` DOES
- USER ASKED: "what is this &1 and then -1"
- PROBLEM: basic bitwise AND not internalized.
- SHOULD KNOW: `X & 1` extracts bit 0. Result is 0 or 1.
- SHOULD KNOW: if bit 0 was added (`| 1`), subtracting 1 removes it.

### E12: DID NOT KNOW WHAT `unlikely()` IS
- USER ASKED: "what is unlikely(head & 1)"
- PROBLEM: compiler hints not known.
- SHOULD KNOW: `unlikely(x)` = branch predictor hint = "x is usually FALSE".
- SHOULD KNOW: `likely(x)` = "x is usually TRUE".
- SOURCE: `include/linux/compiler.h`.

### E13: DID NOT KNOW WHAT `folio` IS
- INTRODUCED: `page_folio(page)` without definition.
- DEFINITION: `folio` = type alias for HEAD page. Guarantees not a tail.
- WHY EXISTS: type safety. `struct folio *` cannot be a tail page.
- USER DID NOT ASK: until error pointed out.

### E14: DID NOT TRACE put_page() REDIRECTION
- USER ASKED: "what happens when someone frees a PFN of compound page"
- ANSWER REQUIRES: following call chain put_page → page_folio → _compound_head → folio_put.
- USER DID NOT: trace this before asking.
- SHOULD HAVE: read mm.h and page-flags.h.

### E15: CONFUSED ABOUT WHY TAIL NEEDS TO POINT TO HEAD
- USER ASKED: "why i need this struct page trick"
- ROOT CAUSE: did not understand that ANY operation on tail must redirect to head.
- EXAMPLES: refcounting, mapping lookup, LRU management, freeing.
- CONSEQUENCE: without link, tail would be orphan.

### E16: DID NOT UNDERSTAND MUTUAL EXCLUSION
- USER ASKED: "can you have buddy_list in a compound page"
- ANSWER: NO. UNION means only one option active at a time.
- USER DID NOT: understand union semantics.
- RULE: if page is TAIL → compound_head is active. if page is FREE → buddy_list is active. NEVER BOTH.

### E17: SKIPPED 2MB DERIVATION
- USER ASKED: "how can i quickly write 2mb in binary"
- SHOULD HAVE DERIVED: 2MB = 2 × 1024 × 1024 = 2 × 2^20 = 2^21.
- SHOULD HAVE DERIVED: 2^21 = 1 followed by 21 zeros in binary.
- USER ASKED INSTEAD OF CALCULATING.

### E18: REQUESTED EXPLANATION INSTEAD OF DOING
- PATTERN: user asks "explain X" instead of tracing X.
- FIX: open source file, read line by line, trace data flow.
- EXAMPLE: asked "explain compound page" instead of reading mm_types.h.

### E19: DID NOT VERIFY VMEMMAP ALIGNMENT GUARANTEE
- USER SAID: "I can just make a struct page* anywhere"
- WRONG: kernel allocates vmemmap at boot. base is aligned. each entry is +64.
- USER DID NOT: check how vmemmap is initialized.
- SOURCE: `arch/x86/mm/init_64.c` or `mm/sparse-vmemmap.c`.

### E20: DID NOT COUNT BITS IN 0x1FFFFF
- FORMULA: `(addr + 0x1FFFFF) & ~0x1FFFFF`
- USER DID NOT: verify 0x1FFFFF = 2^21 - 1 = 21 ones in binary.
- CALCULATION: 0x1FFFFF = 0001 1111 1111 1111 1111 1111 = 21 bits set.
- SHOULD HAVE: counted before asking.

### E21: DID NOT RECOGNIZE | 1 AS + 1 FOR ALIGNED POINTERS
- STORED VALUE: `HEAD | 1`
- EQUIVALENT: `HEAD + 1` (because HEAD bit 0 = 0).
- DECODE: `STORED - 1` = HEAD.
- USER DID NOT: realize | and + are equivalent when bit is clear.

### E22: ASKED SAME QUESTION MULTIPLE TIMES
- "explain compound page from scratch" asked 3+ times.
- INDICATES: not retaining previous explanation.
- FIX: take notes. draw diagrams. trace by hand.

### E23: DID NOT READ KERNEL SOURCE COMMENTS
- mm_types.h has extensive comments explaining each field.
- page-flags.h has comments on compound_head encoding.
- USER RELIED ON: assistant to summarize instead of reading.

### E24: CONFUSED BYTES AND KILOBYTES REPEATEDLY
- 4096 bytes = 4 KB.
- 2048 KB = 2 MB = 2097152 bytes.
- USER CONFUSED: 4096 > 2048 (numbers), forgot UNITS differ.
- FIX: ALWAYS write units. "4096 B" not "4096". "2048 KB" not "2048".

## 09. AXIOMATIC DERIVATION: struct page LAYOUT (Kernel 6.14)

### AXIOM SET:
```
A01: unsigned long = 8 bytes (64-bit system)
A02: unsigned int = 4 bytes
A03: unsigned char = 1 byte
A04: atomic_t = 4 bytes (int with atomic ops)
A05: void * = 8 bytes (64-bit pointer)
A06: struct list_head = 16 bytes (next + prev, each 8 bytes)
```

### OFFSET 0x00: flags
```
LINE 01: flags is declared as `unsigned long`.
LINE 02 (uses A01): unsigned long = 8 bytes.
LINE 03: flags at offset 0x00, size 8 bytes.
LINE 04: Next field starts at 0x00 + 8 = 0x08.
```

### OFFSET 0x08: UNION (lru / compound_head / buddy_list / pcp_list)
```
LINE 05: This is a UNION. All members start at same offset.
LINE 06: Union offset = 0x08.

OPTION A: struct list_head lru
LINE 07 (uses A06): list_head = 16 bytes (next=8, prev=8).
LINE 08: lru.next at 0x08, lru.prev at 0x10.

OPTION B: __filler + mlock_count
LINE 09 (uses A05): __filler = void * = 8 bytes at 0x08.
LINE 10 (uses A02): mlock_count = unsigned int = 4 bytes at 0x10.
LINE 11: Comment says "__filler is always even" → bit 0 = 0.

OPTION C: buddy_list
LINE 12 (uses A06): buddy_list = list_head = 16 bytes at 0x08.

OPTION D: pcp_list
LINE 13 (uses A06): pcp_list = list_head = 16 bytes at 0x08.

OPTION E: compound_head
LINE 14 (uses A01): compound_head = unsigned long = 8 bytes at 0x08.
LINE 15: Bit 0 is set (value = HEAD_ADDR | 1).
LINE 16: Only 8 bytes used (vs 16 for lru), rest undefined.

LINE 17: Union size = max(16, 12, 16, 16, 8) = 16 bytes.
LINE 18: Union ends at 0x08 + 16 = 0x18.
```

### OFFSET 0x18: mapping
```
LINE 19: mapping is declared as `struct address_space *`.
LINE 20 (uses A05): pointer = 8 bytes.
LINE 21: mapping at offset 0x18, size 8 bytes.
LINE 22: Next field at 0x18 + 8 = 0x20.
```

### OFFSET 0x20: index
```
LINE 23: index is declared as `pgoff_t`.
LINE 24: pgoff_t = unsigned long on 64-bit.
LINE 25 (uses A01): unsigned long = 8 bytes.
LINE 26: index at offset 0x20, size 8 bytes.
LINE 27: Next field at 0x20 + 8 = 0x28.
```

### OFFSET 0x28: private
```
LINE 28: private is declared as `unsigned long`.
LINE 29 (uses A01): unsigned long = 8 bytes.
LINE 30: private at offset 0x28, size 8 bytes.
LINE 31: Next field at 0x28 + 8 = 0x30.
```

### OFFSET 0x30: UNION (page_type / _mapcount)
```
LINE 32: This is a UNION of 4-byte fields.

OPTION A: page_type
LINE 33 (uses A02): unsigned int = 4 bytes.

OPTION B: _mapcount
LINE 34 (uses A04): atomic_t = 4 bytes.

LINE 35: Union size = max(4, 4) = 4 bytes.
LINE 36: Union at 0x30, size 4 bytes.
LINE 37: Next field at 0x30 + 4 = 0x34.
```

### OFFSET 0x34: _refcount
```
LINE 38: _refcount is declared as `atomic_t`.
LINE 39 (uses A04): atomic_t = 4 bytes.
LINE 40: _refcount at offset 0x34, size 4 bytes.
LINE 41: Next field at 0x34 + 4 = 0x38.
```

### OFFSET 0x38: memcg_data
```
LINE 42: memcg_data is declared as `unsigned long`.
LINE 43 (uses A01): unsigned long = 8 bytes.
LINE 44: memcg_data at offset 0x38, size 8 bytes.
LINE 45: Struct ends at 0x38 + 8 = 0x40 = 64 bytes.
```

### TOTAL SIZE VERIFICATION:
```
LINE 46: 0x00 + 8 = 0x08 (flags)
LINE 47: 0x08 + 16 = 0x18 (union)
LINE 48: 0x18 + 8 = 0x20 (mapping)
LINE 49: 0x20 + 8 = 0x28 (index)
LINE 50: 0x28 + 8 = 0x30 (private)
LINE 51: 0x30 + 4 = 0x34 (page_type or _mapcount)
LINE 52: 0x34 + 4 = 0x38 (_refcount)
LINE 53: 0x38 + 8 = 0x40 (memcg_data)
LINE 54: Total = 8 + 16 + 8 + 8 + 8 + 4 + 4 + 8 = 64 bytes ✓
LINE 55: 64 = 0x40 ✓
```

### VISUAL LAYOUT:
```
+--------+---------------------+------+----------------------------------+
| OFFSET | FIELD               | SIZE | DERIVATION                       |
+--------+---------------------+------+----------------------------------+
| 0x00   | flags               | 8B   | A01: unsigned long               |
| 0x08   | lru/compound_head   | 16B  | A06: list_head (or 8B for c_head)|
| 0x18   | mapping             | 8B   | A05: pointer                     |
| 0x20   | index               | 8B   | A01: unsigned long (pgoff_t)     |
| 0x28   | private             | 8B   | A01: unsigned long               |
| 0x30   | page_type/_mapcount | 4B   | A02/A04: uint/atomic_t           |
| 0x34   | _refcount           | 4B   | A04: atomic_t                    |
| 0x38   | memcg_data          | 8B   | A01: unsigned long               |
+--------+---------------------+------+----------------------------------+
| 0x40   | TOTAL               | 64B  | Sum verified line by line        |
+--------+---------------------+------+----------------------------------+
```

### NEW THINGS INTRODUCED WITHOUT DERIVATION: NONE

## 10. WHEN COMPOUND PAGES ARE CREATED (CLARIFICATION)

### AXIOM: mmap() vs alloc_pages()

```
mmap() in userspace:
  - Creates Virtual Memory Area (VMA)
  - NO physical memory allocated immediately
  - Physical pages allocated ON DEMAND (page fault)
  - Each fault typically allocates ONE order-0 page
  - Result: independent pages, NOT compound

alloc_pages(order > 0) in kernel:
  - Directly allocates 2^order contiguous pages
  - All pages from buddy allocator as single block
  - prep_compound_page() called
  - Result: compound page (HEAD + TAILs)
```

### LIVE TEST: 9100 bytes via mmap()

```
Request: 9100 bytes
Pages needed: ceil(9100 / 4096) = 3 pages
VMA created: 3 pages virtual address range

Page faults (demand paging):
  touch ptr[0]    → fault → alloc_pages(order=0) → page A
  touch ptr[4096] → fault → alloc_pages(order=0) → page B
  touch ptr[8192] → fault → alloc_pages(order=0) → page C

Result: 3 SEPARATE order-0 pages
  - Page A: standalone (no compound_head)
  - Page B: standalone (no compound_head)
  - Page C: standalone (no compound_head)
  - NOT contiguous in physical memory (likely)
```

### WHEN COMPOUND PAGES ARE CREATED:

```
1. THP (Transparent Huge Pages):
   mmap() + madvise(MADV_HUGEPAGE) + touch large region
   → kernel tries to allocate order-9 (2MB) compound page
   
2. HugeTLB pages:
   mmap() with MAP_HUGETLB flag
   → kernel allocates from hugetlb pool (pre-reserved)
   
3. kmalloc(large size) in kernel:
   kmalloc(size > threshold)
   → may use page allocator directly
   → alloc_pages(order > 0) → compound page
   
4. Kernel internal allocations:
   alloc_pages(GFP_*, order=N) where N > 0
   → always creates compound page
```

### NUMERICAL EXAMPLE: 9100 bytes

```
| Allocation Method      | Physical Layout                      | Compound? |
|------------------------|--------------------------------------|-----------|
| mmap(9100)             | 3 separate order-0 pages             | NO        |
| mmap(9100)+MADV_HUGEPAGE| 1 order-9 (512 pages) if possible   | YES       |
| kmalloc(9100)          | slab or order-2 (4 pages)           | MAYBE     |
| alloc_pages(order=2)   | 4 contiguous pages                   | YES       |
```

### WASTE CALCULATION (if compound):

```
Request: 9100 bytes
Pages needed: 3
Buddy allocator gives: 2^ceil(log2(3)) = 4 pages
Bytes allocated: 4 × 4096 = 16384 bytes
Waste: 16384 - 9100 = 7284 bytes (44%)
```

### ERROR ADDED: E25

```
E25: CONFUSED mmap() WITH alloc_pages()
- USER THOUGHT: mmap(9100) creates compound page
- REALITY: mmap() creates VMA, page faults allocate individual pages
- COMPOUND CREATION: only via alloc_pages(order>0) or THP/HugeTLB
```

## 11. CODING ERRORS (TODO 6 SESSION)

### E26: COMMA INSTEAD OF OR
- LINE: 237
- WROTE: `mmap(NULL, 1<<24, PROT_READ, PROT_WRITE, ...)`
- SHOULD BE: `mmap(NULL, SIZE, PROT_READ | PROT_WRITE, ...)`
- CAUSE: comma creates 7 arguments, mmap takes 6
- SLOPPY: did not re-read function signature
- PREVENT: always check man page before calling

### E27: WRONG EXPONENT FOR SIZE
- LINE: 237
- WROTE: `1 << 24` = 16777216 = 16 MB
- SHOULD BE: `1 << 22` = 4194304 = 4 MB
- CAUSE: did not calculate 4 MB = 4 × 1024 × 1024 = 4194304 = 2^22
- SLOPPY: guessed exponent instead of deriving
- PREVENT: always derive: 4 MB = 4 × 2^20 = 2^2 × 2^20 = 2^22

### E28: DID NOT UNDERSTAND ALIGNMENT FORMULA
- LINE: 244
- FORMULA: `(raw + (1<<21) - 1) & ~((1<<21) - 1)`
- ASKED: "why add and then AND?"
- MISSED: add pushes UP, AND drops back DOWN to boundary
- PREVENT: trace with example: 5 + 7 = 12, 12 & ~7 = 8

### E29: DID NOT UNDERSTAND WHY 4 MB FOR 2 MB HUGEPAGE
- ASKED: "why not madvise same size as mmap?"
- REASON: mmap may return non-aligned address
- NEED: 2 MB aligned start + 2 MB space = worst case 4 MB
- SLOPPY: did not draw memory diagram before asking

### E30: DID NOT VERIFY ALIGNED REGION FITS IN ALLOCATION
- ASKED: "how do we know address is valid?"
- PROOF: 4 MB > (max gap before aligned) + 2 MB
- CALCULATION: max gap = 2 MB - 1 byte, total < 4 MB
- SLOPPY: asked before calculating

### E31: CONFUSED KB AND MB IN madvise SIZE
- ASKED: "but my page of 4 MB asked"
- CONFUSION: mmap(4 MB) for room, madvise(2 MB) for huge page
- RULE: huge page = 2 MB fixed, extra allocation = alignment buffer
