01. DEFINE RAM: Random Access Memory is a linear array of bytes indexed by "Physical Address" from 0 to Size-1 → CPU reads/writes bytes at specific addresses.
02. DEFINE PAGE_SIZE: Managing individual bytes is too slow → Hardware groups bytes into fixed "Pages" → Standard size is 4096 bytes (0x1000) → Derived from Axiom A01 (getconf).
03. DEFINE PAGE FRAME: A physical page of RAM → Starts at Address A where A % 4096 == 0.
04. DEFINE PFN: Page Frame Number → Index of the page in the RAM array → Formula: PFN = Physical Address / PAGE_SIZE.
05. DEFINE ZONE: Physical RAM is split into ranges called "Zones" due to hardware limits (e.g. DMA < 16MB) → Zone is a range of PFNs [Start, End).
06. DEFINE ORDER: Allocator manages blocks of 2^N contiguous pages → "Order N" means a block of size 2^N pages.
07. DEFINE BUDDY INFO: Kernel tracks free blocks of each Order per Zone → `/proc/buddyinfo` lists these counts.
08. AXIOM REAL DATA RAM: Your machine has MemTotal 15776272 kB (Ax: /proc/meminfo) → Bytes = 16,154,902,528 → Pages = 16,154,902,528 / 4096 = 3,944,068 Total Pages.
09. AXIOM REAL DATA ZONE: Your machine has "Node 0 Zone DMA32" (Ax: /proc/buddyinfo) → DMA32 is zone for 32-bit addressable RAM (< 4GB).
10. AXIOM REAL DATA ORDER-0: BuddyInfo shows DMA32 has 24581 blocks at Order 0 → 24581 Free Single Pages available in this zone.
11. DEFINE STRUCT PAGE: Kernel needs metadata (flags, count) for every physical page → "struct page" is 64-byte object (Ax: Sizeof).
12. DEFINE VMEMMAP: Array of `struct page` for all PFNs → `vmemmap[PFN]` stores metadata for Physical Page `PFN`.
13. DERIVE FORMULA PFN-TO-PAGE: Address of struct page `p` = VMEMMAP_BASE + (PFN × 64).
14. DERIVE FORMULA PAGE-TO-PFN: PFN = (Address of `p` - VMEMMAP_BASE) / 64.
15. ACTION ALLOCATE PAGE ALLOC: Call `alloc_page(GFP_KERNEL)` → Kernel finds a free Order-0 block (e.g. one of the 24581 in DMA32) → Returns pointer `p`.
16. TRACE POINTER CALCULATION: Assume `p` returned is `0xFFFFEA0000040000` (Axiomatic Example) → Calculate Offset = `0xFFFFEA0000040000 - 0xFFFFEA0000000000` = 0x40000 (262144 bytes).
17. TRACE PFN CALCULATION: PFN = Offset / 64 = 262144 / 64 = 4096.
18. TRACE PHYS CALCULATION: Phys = PFN × 4096 = 4096 × 4096 = 16,777,216 (0x1000000) → 16MB boundary (Start of DMA32).
19. VERIFY ZONE: 16MB is start of DMA32 Zone → Matches "Zone DMA32" from BuddyInfo Axiom ✓.
20. DEFINE REFCOUNT: `_refcount` is atomic counter at offset 52 in struct page → Tracks usage.
21. TRACE REFCOUNT ALLOC: `alloc_page` sets `_refcount = 1` → Means "Allocated".
22. TRACE REFCOUNT GET: `get_page` adds 1 → `_refcount = 2`.
23. TRACE REFCOUNT PUT: `put_page` subtracts 1 → `_refcount = 1` → Not zero ∴ Page not freed.
24. CODE TASK: Implement `page_labs.c` to Allocate, Print PFN (using derived formula), Print Phys, Get/Put, and Free.
