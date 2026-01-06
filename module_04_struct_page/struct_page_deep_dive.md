=== STRUCT PAGE DEEP DIVE ===
=== LINE BY LINE AXIOMATIC DERIVATION ===

01. PREVIOUSLY DERIVED: RAM = array of bytes → 16GB = 16,154,902,528 bytes.
02. PREVIOUSLY DERIVED: PAGE = 4096 bytes → RAM has 3,944,068 pages.
03. PREVIOUSLY DERIVED: PFN = index of page → PFN 0 = bytes 0-4095, PFN 1 = bytes 4096-8191.
04. PROBLEM: Kernel needs to track STATE of each page → Which pages are free? Which are dirty? Which are locked?

=== PARAGRAPH 1: "In order to manage pages of memory we must store metadata about them" ===

05. DERIVE METADATA NEED: 3,944,068 pages → Each page needs: is_free? is_dirty? is_locked? who_owns_it? → Multiple bits/bytes per page.
06. DEFINE METADATA: Data about data → For pages: flags, refcount, mappings → Stored separately from page content.
07. PROBLEM METADATA LOCATION: Store metadata WHERE? → Inside the 4096-byte page? NO → Would waste usable memory.
08. SOLUTION SEPARATE ARRAY: Store metadata in separate array → One entry per page → Array indexed by PFN.

=== PARAGRAPH 2: "flags to identify what characteristics the page has" ===

09. DEFINE FLAGS: 64-bit integer → Each bit = one attribute → Bit 0 = locked, Bit 4 = dirty, Bit 5 = lru.
10. AXIOM FLAGS SOURCE: Kernel source page-flags.h:94 → enum pageflags { PG_locked=0, PG_dirty=4, PG_lru=5, ... }.
11. DERIVE FLAGS SIZE: 64 bits = 8 bytes → Stored at offset 0 in struct page.

=== PARAGRAPH 3: "reference count to determine whether we can free the page or not" ===

12. DEFINE REFCOUNT: Counter tracking users of page → alloc sets to 1 → get_page adds 1 → put_page subtracts 1.
13. PROBLEM REFCOUNT SOLVES: Page shared by 3 processes → When can kernel free it? → Only when refcount=0.
14. AXIOM REFCOUNT TYPE: atomic_t _refcount → 4 bytes → At offset 52 in struct page.
15. DERIVE REFCOUNT ACCESS: page_ref_count(page) → Returns current value → DO NOT access _refcount directly.

=== PARAGRAPH 4: "The data type that encapsulates this data is struct page" ===

16. DEFINE STRUCT PAGE: C structure containing all page metadata → One struct page per physical page frame.
17. AXIOM STRUCT PAGE SIZE: 64 bytes on x86_64 → Matches L1 cache line size → Efficient access.
18. AXIOM STRUCT PAGE SOURCE: Kernel source mm_types.h:72 → struct page { unsigned long flags; ... }.
19. DERIVE TOTAL OVERHEAD: 3,944,068 pages × 64 bytes = 252,420,352 bytes = 241 MB for metadata.

=== STRUCT PAGE LAYOUT (Figure 2-2) ===

20. OFFSET 0-7: unsigned long flags → 8 bytes → Page attributes + zone encoding.
21. OFFSET 8-47: Metadata union → 40 bytes → Changes based on page type (anonymous, file, slab).
22. OFFSET 48-51: Union of _mapcount (4 bytes) or page_type (4 bytes).
23. OFFSET 52-55: atomic_t _refcount → 4 bytes → Reference count.
24. OFFSET 56-63: unsigned long memcg_data → 8 bytes → Memory cgroup pointer.
25. VERIFY: 8 + 40 + 4 + 4 + 8 = 64 bytes ✓.

=== FLAGS FIELD DETAIL ===

26. FLAGS BITS 0-21: Page attributes → PG_locked=0, PG_dirty=4, PG_lru=5, PG_head=6, etc.
27. FLAGS BITS 60-61: Node ID (NUMA) → Your machine: single node → Node=0.
28. FLAGS BITS 62-63: Zone ID → 0=DMA, 1=DMA32, 2=Normal → Your pages mostly Zone=2.
29. AXIOM ZONE ACCESS: page_zonenum(page) → Extracts zone from flags → (flags >> 62) & 0x3.
30. AXIOM NODE ACCESS: page_to_nid(page) → Extracts node from flags.

=== METADATA UNION: WHY UNION? ===

31. DEFINE UNION: C construct → Multiple types share same memory → Only one active at a time.
32. PROBLEM SPACE EFFICIENCY: Different page types need different metadata → Anonymous needs lru, slab needs slab_list.
33. SOLUTION UNION: Overlay different structs at same offset → Saves space → page->lru and page->slab_list cannot coexist.
34. DERIVE UNION SIZE: Largest member = 40 bytes → Union size = 40 bytes.

=== METADATA UNION CASE 1: ANONYMOUS/PAGE CACHE PAGES ===

35. DEFINE ANONYMOUS PAGE: Page not backed by file → Heap (malloc), stack, COW pages.
36. DEFINE PAGE CACHE PAGE: Page backed by file → mmap'd file, read() cache.
37. ANON/CACHE LAYOUT OFFSET 8-23: struct list_head lru → 16 bytes → Connects page to LRU list.
38. ANON/CACHE LAYOUT OFFSET 24-31: struct address_space *mapping → 8 bytes → Points to file's address_space.
39. ANON/CACHE LAYOUT OFFSET 32-39: pgoff_t index → 8 bytes → Offset within file (in pages).
40. ANON/CACHE LAYOUT OFFSET 40-47: unsigned long private → 8 bytes → Filesystem-specific data.

=== DEFINE LRU LIST ===

41. DEFINE LRU: Least Recently Used → List ordered by access time → Old pages at end.
42. PROBLEM LRU SOLVES: Memory pressure → Which pages to evict? → Evict least recently used.
43. DEFINE LIST_HEAD: struct { prev*, next* } → 16 bytes → Doubly-linked list.
44. DERIVE LRU CONNECTION: page->lru.next points to next page's lru → Chain of pages.

=== DEFINE MAPPING FIELD ===

45. DEFINE ADDRESS_SPACE: Kernel struct for file's page cache → Contains radix tree of cached pages.
46. MAPPING LOW BITS: Address must be aligned → Low 2 bits available for flags.
47. MAPPING BIT 0: PAGE_MAPPING_ANON → If set, mapping points to anon_vma, not address_space.
48. DERIVE: Anonymous page → mapping = anon_vma | PAGE_MAPPING_ANON → File page → mapping = address_space.

=== MAPCOUNT FIELD ===

49. DEFINE MAPCOUNT: Count of page table entries pointing to this page.
50. PROBLEM MAPCOUNT SOLVES: Same physical page mapped in multiple processes → How many PTEs reference it?
51. INITIAL VALUE: _mapcount = -1 → Means "not mapped" → 0 means "mapped once" → 1 means "mapped twice".
52. DERIVE REAL COUNT: Actual mappings = _mapcount + 1 → If _mapcount=2 → 3 PTEs point here.

=== PAGE_TYPE FIELD (SAME LOCATION AS MAPCOUNT) ===

53. UNION _MAPCOUNT/PAGE_TYPE: Same 4 bytes at offset 48 → Cannot use both simultaneously.
54. WHEN MAPCOUNT: Page mappable in userland → _mapcount tracks PTEs.
55. WHEN PAGE_TYPE: Kernel-only page (slab, page table) → page_type identifies type.
56. AXIOM PAGE_TYPE VALUES: See page-flags.h → PG_buddy, PG_offline, PG_table, etc.

=== COMPOUND PAGES ===

57. DEFINE COMPOUND PAGE: Multiple contiguous pages allocated as single unit → 2^order base pages.
58. PROBLEM COMPOUND SOLVES: Need 16KB? → Allocate order-2 compound = 4 × 4KB pages.
59. DEFINE HEAD PAGE: First struct page of compound → Has PG_head flag set.
60. DEFINE TAIL PAGE: Remaining struct pages → compound_head field points to head | 1.
61. DERIVE TAIL MARKER: tail->compound_head = (head_address | 1) → Low bit set = "I am tail".
62. AXIOM COMPOUND_HEAD: compound_head(page) → If tail, returns head → If head/single, returns itself.

=== TAIL PAGE METADATA (FIRST TAIL) ===

63. FIRST TAIL OFFSET 8: compound_head → Pointer to head | 1.
64. FIRST TAIL OFFSET 16: compound_dtor (1 byte) → Destructor ID for freeing.
65. FIRST TAIL OFFSET 17: compound_order (1 byte) → Order of compound (0-10).
66. FIRST TAIL OFFSET 18-21: compound_mapcount → Mapping count for whole compound.
67. FIRST TAIL OFFSET 22-25: compound_pincount → Pin count for compound.
68. FIRST TAIL OFFSET 26-29: compound_nr → Number of base pages = 2^order.

=== COMPOUND ORDER ===

69. AXIOM MAX_ORDER: Maximum order = 10 → Max compound = 2^10 = 1024 pages = 4MB.
70. DERIVE ORDER SIZES: order=0→1 page=4KB, order=1→2 pages=8KB, order=2→4 pages=16KB, order=10→1024 pages=4MB.
71. AXIOM __GFP_COMP: Must set this flag to get compound page → Without it, just N separate pages.

=== SLAB PAGES ===

72. DEFINE SLAB: Memory allocator for small objects → kmalloc uses slab.
73. PROBLEM SLAB SOLVES: Need 128 bytes? → Don't waste full 4KB page → Carve page into 128-byte chunks.
74. SLAB OVERLAY: struct slab shares memory with struct page → Same 64 bytes, different interpretation.
75. SLAB FIELDS: slab_cache (pointer to kmem_cache), freelist, slab_list, etc.

=== STRUCT FOLIO ===

76. DEFINE FOLIO: New type representing non-tail page → Always head or single page.
77. PROBLEM FOLIO SOLVES: Functions passed struct page* → Is it head or tail? → Must call compound_head() everywhere.
78. SOLUTION FOLIO: struct folio* guarantees not tail → No compound_head() needed → Cleaner code.
79. FOLIO LAYOUT: Contains union with struct page → Easy conversion via page_folio()/folio_page().

=== DO BY HAND ===

80. DO: Calculate struct page array size → 3,944,068 × 64 = ___ bytes = ___ MB.
81. DO: Given _mapcount=5 → Actual mappings = 5 + 1 = ___.
82. DO: Order-3 compound → Pages = 2^3 = ___ → Size = ___ × 4096 = ___ bytes.
83. DO: Tail page compound_head = 0xFFFFEA0000100001 → Head address = 0x___ & ~1 = 0x___.

=== FAILURE PREDICTIONS ===

84. F01: Read _mapcount directly → Wrong! Use page_mapcount() → Returns _mapcount + 1.
85. F02: Read _refcount directly → Wrong! Use page_ref_count().
86. F03: Assume struct page = 64 bytes always → Can vary by config → Check on your kernel.
87. F04: Confuse page_type with _mapcount → Same offset, different use cases.
88. F05: Access tail page fields directly → Wrong! Use compound_head() first.
89. F06: Allocate high-order without __GFP_COMP → Not compound, just separate pages.
90. F07: Confuse folio with page → Folio never tail, page might be tail.
