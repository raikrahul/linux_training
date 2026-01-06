01. D01 DEFINE COMPOUND: Compound page = 2^order contiguous pages allocated with __GFP_COMP flag → Tracked as single unit.
02. D02 PROBLEM COMPOUND SOLVES: Need large contiguous memory (e.g. 16KB) → Allocate 4 pages together → Track as one.
03. D03 DEFINE HEAD: First struct page of compound → Has PG_head flag set → PageHead(p)==true.
04. D04 DEFINE TAIL: Remaining struct pages → compound_head field = head_address | 1 → PageTail(p)==true.
05. D05 FORMULA ORDER: order=N → 2^N pages → order=2 → 4 pages → 1 head + 3 tails.
06. ACTION ALLOCATE: alloc_pages(GFP_KERNEL | __GFP_COMP, 2) → Returns head → FILL head=0x___ (from dmesg).
07. DERIVE TAIL1: tail1 = head + 1 → Pointer arithmetic → tail1 address = head + 64 bytes = 0x___ + 0x40 = 0x___.
08. DERIVE PFN: head_pfn = page_to_pfn(head) = 0x___ → tail1_pfn = head_pfn + 1 = 0x___.
09. VERIFY: Is tail1_pfn == head_pfn + 1? → ___ == ___ + 1 → ✓/✗.
10. TRACE COMPOUND_HEAD: tail1->compound_head = head | 1 → FILL: 0x___ | 0x1 = 0x___.
11. DERIVE HEAD FROM TAIL: compound_head(tail1) = tail1->compound_head & ~1 = 0x___ & ~1 = 0x___.
12. VERIFY: compound_head(tail1) == head? → 0x___ == 0x___ → ✓/✗.
13. TRACE ORDER: compound_order(head) reads head[1].compound_order → FILL: ___ (Expected: 2).
14. FAILURE PREDICTION F01: Free tail page with __free_pages(tail1, 0) → Wrong! Must free head with correct order.
15. FAILURE PREDICTION F02: Access tail[3] for order=2 compound → Only 3 tails exist (indices 1,2,3) → tail[3] is last.
16. FAILURE PREDICTION F03: Confuse compound_head pointer value (has |1) with actual head address → Must mask off low bit.
17. FAILURE PREDICTION F04: Allocate order=2 without __GFP_COMP → 4 pages allocated but NOT compound → No head/tail setup.
