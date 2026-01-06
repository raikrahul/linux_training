01. D01 DEFINE REFCOUNT: `atomic_t _refcount` at offset 52 (0x34) in struct page → 4 bytes → Tracks page ownership.
02. D02 PROBLEM REFCOUNT SOLVES: Multiple users share one physical page → Need counter to know when safe to free.
03. D03 AXIOM ALLOC: alloc_page(GFP_KERNEL) → Sets _refcount = 1 → Meaning: Allocator owns page.
04. D04 AXIOM GET: get_page(p) → atomic_inc(&p->_refcount) → refcount += 1 → Meaning: "I am using this page".
05. D05 AXIOM PUT: put_page(p) → atomic_dec_and_test(&p->_refcount) → If result=0, free page → Meaning: "I am done".
06. ACTION ALLOCATE: alloc_page(GFP_KERNEL) → FILL refcount=___ (from dmesg, Expected: 1).
07. ACTION GET: get_page(p) → CALCULATE: refcount = 1 + 1 = ___ → FILL from dmesg: ___ (Expected: 2).
08. ACTION PUT 1: put_page(p) → CALCULATE: refcount = 2 - 1 = ___ → Is 1=0? NO → Page NOT freed.
09. ACTION PUT 2: put_page(p) → CALCULATE: refcount = 1 - 1 = ___ → Is 0=0? YES → Page FREED.
10. BUG TRACE: After step 9, page is freed → p points to FREE memory → Reading p->_refcount is Use-After-Free.
11. BUG SYMPTOM: dmesg shows refcount=0 or -1 or garbage after second put_page → DANGER.
12. BUG TRIGGER: If we call __free_pages(p, 0) after step 9 → DOUBLE FREE → Kernel BUG_ON or corruption.
13. FIX ANALYSIS: Count operations: alloc(+1), get(+1), put(-1), put(-1) = 0 → Page freed by second put.
14. FIX ANALYSIS: Should be: alloc(+1), get(+1), put(-1), __free_pages(-1) = 0 → __free_pages frees.
15. FIX ANALYSIS: OR: alloc(+1), __free_pages(-1) = 0 → No get/put at all → Simpler.
16. FAILURE PREDICTION F01: Forget get_page was called → Call only one put_page → refcount stuck at 1 → LEAK.
17. FAILURE PREDICTION F02: Call __free_pages after put_page already freed → DOUBLE FREE → BUG_ON.
18. FAILURE PREDICTION F03: Read page->_refcount after freed → Use-After-Free → Garbage value or crash.
