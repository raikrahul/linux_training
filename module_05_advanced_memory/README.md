01. AXIOM STRUCT PAGE: 64-byte structure at vmemmap[PFN] → Holds metadata for physical page PFN.
02. AXIOM STRUCT PAGE LAYOUT: Offset 0-7: flags (8B). Offset 8-47: metadata union (40B). Offset 48-51: _mapcount/page_type (4B). Offset 52-55: _refcount (4B). Offset 56-63: memcg_data (8B).
03. VERIFY: 8 + 40 + 4 + 4 + 8 = 64 bytes ✓.
04. DEFINE UNION: C construct → Multiple structs share same memory → Only ONE active at a time.
05. PROBLEM UNION SOLVES: Different page types need different metadata → Anonymous needs lru list. Slab needs freelist. Page cache needs mapping.
06. SOLUTION: Overlay different structs at offset 8-47 → Same 40 bytes, different interpretation.

=== METADATA UNION CASES ===

07. CASE 1 ANONYMOUS/PAGE CACHE: Page allocated for process heap/stack OR file cache.
08. LAYOUT CASE 1 OFFSET 8-23: struct list_head lru → 16 bytes → {prev*, next*} → Links page to LRU list.
09. LAYOUT CASE 1 OFFSET 24-31: struct address_space *mapping → 8 bytes → Points to file's page cache OR anon_vma.
10. LAYOUT CASE 1 OFFSET 32-39: pgoff_t index → 8 bytes → Offset within file (in pages) OR swap entry.
11. LAYOUT CASE 1 OFFSET 40-47: unsigned long private → 8 bytes → Filesystem-specific data.
12. VERIFY CASE 1: 16 + 8 + 8 + 8 = 40 bytes ✓.

13. CASE 2 SLAB PAGE: Page used by slab allocator for small objects (kmalloc).
14. PROBLEM SLAB: Need 128-byte object? Don't waste 4096-byte page. Carve into 32 chunks of 128 bytes.
15. LAYOUT CASE 2: Whole struct page cast to struct slab → Different field names, same offsets.
16. SLAB SHARES: flags (offset 0), _refcount (offset 52), memcg_data (offset 56).
17. SLAB OVERWRITES: metadata union (offset 8-47) AND _mapcount/page_type (offset 48-51).

18. CASE 3 COMPOUND TAIL PAGE: Page is tail of compound (multi-page) allocation.
19. LAYOUT CASE 3 OFFSET 8-15: unsigned long compound_head → Pointer to head page | 1.
20. LAYOUT CASE 3 OFFSET 16-17: unsigned char compound_dtor, compound_order → Destructor ID, order (0-10).
21. LAYOUT CASE 3 OFFSET 18-21: atomic_t compound_mapcount → Mapping count for compound.
22. LAYOUT CASE 3 OFFSET 22-25: atomic_t compound_pincount → Pin count.
23. LAYOUT CASE 3 OFFSET 26-29: unsigned int compound_nr → Number of pages = 2^order.

=== HOW TO DETECT WHICH CASE ===

24. DETECTION RULE: Check flags and field values to determine case.
25. IF PageSlab(page): page->flags has PG_slab bit set → Case 2.
26. IF PageTail(page): page->compound_head has low bit set → Case 3.
27. IF PageAnon(page): page->mapping has PAGE_MAPPING_ANON bit set → Anonymous.
28. IF page->mapping points to address_space: → Page cache.

=== KERNEL MODULE PLAN ===

29. ALLOCATE PAGE: alloc_page(GFP_KERNEL) → Returns normal page → Case 1 (anonymous).
30. READ OFFSET 8-23: Print lru.prev and lru.next → Should be NULL or list head.
31. READ OFFSET 24-31: Print mapping → Should be NULL for freshly allocated.
32. READ OFFSET 32-39: Print index → Should be 0.
33. ALLOCATE SLAB OBJECT: kmalloc(128, GFP_KERNEL) → Returns pointer from slab page.
34. FIND SLAB PAGE: virt_to_page(kmalloc_ptr) → Get struct page of slab.
35. CHECK PG_slab: PageSlab(page) → Should be true.
36. COMPARE: Same offset bytes interpreted differently.

=== DO BY HAND ===

37. DO: RUN `sudo insmod metadata_union.ko`.
38. DO: RUN `sudo dmesg | tail -30` → Read output.
39. DO: For normal page, read lru.prev = 0x___, lru.next = 0x___.
40. DO: For normal page, read mapping = 0x___, index = ___.
41. DO: For slab page, check PageSlab() = ___ (expected: 1).
42. DO: CALCULATE offset of lru in struct page: offsetof(struct page, lru) = ___ bytes.
43. DO: CALCULATE offset of mapping in struct page: offsetof(struct page, mapping) = ___ bytes.

=== FAILURE PREDICTIONS ===

44. F01: Access lru on slab page → WRONG! Slab uses different fields at same offset.
45. F02: Assume all pages have valid mapping → NO! Freshly allocated pages have mapping=NULL.
46. F03: Confuse page->mapping with page address → mapping is pointer to address_space, not page.
47. F04: Forget low bits of mapping have flags → PAGE_MAPPING_ANON = bit 0.

=== VIOLATION CHECK ===

48. Lines 01-06: Axioms and definitions. Lines 07-23: Case layouts. Lines 24-28: Detection. Lines 29-36: Plan. Lines 37-43: DO BY HAND. Lines 44-47: Failures.
49. NEW THINGS INTRODUCED WITHOUT DERIVATION: NONE → Each term defined before use ✓.

=== REAL OUTPUT FROM YOUR MACHINE ===

50. NORMAL PAGE: page=0xfffff7da455024c0, PFN=0x154093=1,392,787.
51. NORMAL offset 8-15: lru.prev=0xdead000000000122 → LIST_POISON1 (not linked to any list).
52. NORMAL offset 16-23: lru.next=0x0000000000000000 → NULL (not linked).
53. NORMAL offset 24-31: mapping=0x0000000000000000 → NULL (not assigned to address_space).
54. NORMAL PageSlab()=0 ✓ → Uses lru/mapping/index interpretation.

55. SLAB PAGE: page=0xfffff7da44236bc0, PFN=0x108daf=1,085,871.
56. SLAB offset 8-15: [raw]=0xdead000000000122 → Interpreted as lru.prev, but actually slab field.
57. SLAB offset 16-23: [raw]=0xffff8cf5c0047600 → Valid kernel pointer! This is freelist, NOT lru.next.
58. SLAB PageSlab()=1 ✓ → Uses slab_cache/freelist interpretation.

59. KEY PROOF: Same offset 16-23 has DIFFERENT values.
60. Normal: 0x0000000000000000 (NULL lru.next).
61. Slab: 0xffff8cf5c0047600 (valid freelist pointer).
62. ∴ Same 40 bytes, DIFFERENT meanings based on page type.

