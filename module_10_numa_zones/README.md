01. D01 DEFINE FLAGS: `unsigned long flags` → 8 bytes → Offset 0 in struct page → Source: mm_types.h:73.
02. D02 DEFINE ZONES_SHIFT: Zone bits at position 62-63 → ZONES_SHIFT=62 → Source: page-flags-layout.h.
03. D03 DERIVE ZONE EXTRACTION: zone = (flags >> 62) & 0x3 → 2 bits → Values 0,1,2,3.
04. D04 DEFINE ZONE VALUES: ZONE_DMA=0, ZONE_DMA32=1, ZONE_NORMAL=2 → Source: mmzone.h:759,762,769.
05. D05 DEFINE NODE SHIFT: Node bits at position 60-61 → NODES_SHIFT=60 → For NUMA machines.
06. D06 DERIVE NODE EXTRACTION: node = (flags >> 60) & NODES_MASK → Your machine: single node → node=0 always.
07. D07 PROBLEM: Old ISA DMA controller has 24-bit address bus → 2^24=16777216 bytes=16MB → Cannot access RAM > 16MB.
08. D08 SOLUTION DMA ZONE: Label pages with address < 16MB as "DMA zone" → Old devices request from this zone.
09. D09 DERIVE DMA BOUNDARY: 16MB / 4096 = 4096 PFNs → PFN < 4096 = DMA zone.
10. D10 PROBLEM: 32-bit PCI devices have 32-bit address bus → 2^32=4GB → Cannot access RAM > 4GB.
11. D11 SOLUTION DMA32 ZONE: Label pages with address < 4GB as "DMA32 zone" → 32-bit devices request from here.
12. D12 DERIVE DMA32 BOUNDARY: 4GB / 4096 = 1048576 PFNs → 4096 <= PFN < 1048576 = DMA32 zone.
13. D13 DEFINE NORMAL ZONE: Pages with PFN >= 1048576 → Normal zone → For kernel and modern devices.
14. D14 DEFINE GFP_KERNEL: Allocation flag = 0xCC0 → Normal context → Prefers Normal zone.
15. D15 DEFINE GFP_DMA32: Allocation flag = 0x04 → Forces allocation from DMA32 zone.
16. D16 DEFINE GFP_DMA: Allocation flag = 0x01 → Forces allocation from DMA zone.
17. D17 FUNCTION page_zonenum(p): Returns zone from flags → (p->flags >> ZONES_SHIFT) & ZONES_MASK.
18. D18 FUNCTION page_to_nid(p): Returns node from flags → (p->flags >> NODES_SHIFT) & NODES_MASK.

=== DO BY HAND ===

19. DO: RUN `make` → Compile module.
20. DO: RUN `sudo insmod flags_zone_node.ko` → Load module.
21. DO: RUN `sudo dmesg | tail -40` → See output.
22. DO: For ALLOC1, read PFN=0x___ from output → CALCULATE: Is 0x___ >= 0x100000 (1048576)? → ___ ✓/✗.
23. DO: For ALLOC1, verify zone from flags manually → (flags >> 62) & 0x3 = ___ → Compare to page_zonenum() output.
24. DO: For ALLOC2, read PFN=0x___ → CALCULATE: Is 0x___ < 0x100000? → ___ ✓/✗ → Zone should be DMA32 or DMA.
25. DO: For ALLOC3, read PFN=0x___ → CALCULATE: Is 0x___ < 0x1000 (4096)? → ___ ✓/✗ → Zone should be DMA.
26. DO: RUN `sudo rmmod flags_zone_node` → Unload module.

=== FAILURE PREDICTIONS ===

27. F01: Confuse zone number (0,1,2) with zone name → 0=DMA, 1=DMA32, 2=Normal.
28. F02: Wrong shift for zone → (flags >> 60) instead of (flags >> 62) → Gets node+zone mixed.
29. F03: Expect GFP_KERNEL to always return Normal zone → Can return DMA32 if Normal exhausted.
30. F04: Expect GFP_DMA to always succeed → DMA zone tiny (~16MB) → May fail if exhausted.
31. F05: Confuse PFN boundary with physical address boundary → DMA32 ends at PFN 1048576, not address 1048576.

=== VIOLATION CHECK ===

32. Lines 01-18: Definitions. Lines 19-26: DO BY HAND. Lines 27-31: Failures.
33. NEW THINGS INTRODUCED WITHOUT DERIVATION: NONE → Each term defined before use ✓.

=== REAL OUTPUT FROM YOUR MACHINE ===

34. ALLOC1 GFP_KERNEL: p=0xfffff7da45f69dc0, PFN=0x17da77=1,563,255, zone=2(Normal), node=0.
35. ALLOC2 GFP_DMA32: p=0xfffff7da4024cc00, PFN=0x9330=37,680, zone=1(DMA32), node=0.
36. ALLOC3 GFP_DMA: p=0xfffff7da40002700, PFN=0x9c=156, zone=0(DMA), node=0.

37. VERIFY ALLOC1: PFN=1,563,255 → 1,563,255 >= 1,048,576? → 1,563,255 > 1,048,576 ✓ → Normal zone.
38. VERIFY ALLOC2: PFN=37,680 → 4,096 <= 37,680 < 1,048,576? → ✓ → DMA32 zone.
39. VERIFY ALLOC3: PFN=156 → 156 < 4,096? → 156 < 4,096 ✓ → DMA zone.

40. BUG DISCOVERED: Manual (flags >> 62) & 0x3 = 0 ≠ page_zonenum() = 2.
41. REASON: Zone bits NOT at position 62-63 on Kernel 6.14 → Use page_zonenum() API, not manual extraction.
42. LESSON: Never manually extract zone from flags → Use page_zonenum(page) which knows correct shift.

43. PHYSICAL ADDRESS CALC: 
44. ALLOC1: Phys = PFN × 4096 = 0x17da77 × 0x1000 = 0x17da77000 = 6,378,975,232 bytes = 5.9 GB into RAM.
45. ALLOC2: Phys = PFN × 4096 = 0x9330 × 0x1000 = 0x9330000 = 154,271,744 bytes = 147 MB into RAM.
46. ALLOC3: Phys = PFN × 4096 = 0x9c × 0x1000 = 0x9c000 = 638,976 bytes = 624 KB into RAM.

=== WHY page_to_pfn() WORKS (AXIOMATIC DERIVATION) ===

47. AXIOM RAM: RAM = array of bytes. Byte 0, Byte 1, ... Byte 16,154,902,527.
48. DEFINE PAGE: Contiguous 4096 bytes. Page 0 = bytes 0-4095. Page 1 = bytes 4096-8191.
49. DEFINE PFN: Page Frame Number = index of page. PFN 0 = Page 0. PFN 1 = Page 1.
50. PROBLEM: Kernel needs 64-byte metadata struct for EACH page. Where to store?
51. CALCULATE: 3,944,068 pages × 64 bytes = 252,420,352 bytes = 241 MB needed.
52. KERNEL ACTION AT BOOT: Reserve physical RAM[0x10000000..0x1F0ED400] for struct page array.
53. RESULT: struct page[0] at physical 0x10000000. struct page[1] at physical 0x10000040. struct page[N] at physical 0x10000000 + N×64.
54. AXIOM VMEMMAP: Kernel picks virtual address 0xFFFFEA0000000000 as name for this array.
55. KERNEL ACTION: Create page table mapping: Virtual 0xFFFFEA0000000000+OFFSET → Physical 0x10000000+OFFSET.
56. KEY INSIGHT: Virtual offset from base = Physical offset from base. SAME offset in both spaces.
57. DERIVE page_to_pfn: Given p (virtual pointer to struct page).
58. STEP 1: offset = p - vmemmap_base = p - 0xFFFFEA0000000000.
59. STEP 2: array_index = offset / 64.
60. STEP 3: BY DESIGN, array[index] stores metadata for PFN=index.
61. ∴ PFN = (p - vmemmap_base) / 64. Pure pointer arithmetic. No MMU lookup.
62. EXAMPLE: p = 0xFFFFEA0008F7A300 → offset = 0x8F7A300 → index = 0x8F7A300/64 = 0x23DE8C = 2,350,732 → PFN = 2,350,732.
63. VERIFY: PFN 2,350,732 × 64 = 150,446,848 = 0x8F7A300 → 0xFFFFEA0000000000 + 0x8F7A300 = 0xFFFFEA0008F7A300 ✓.
64. THIS IS NOT MMU TRANSLATION: This is pointer subtraction on KNOWN array layout designed by kernel.
65. F08: Confuse "pointer arithmetic gives PFN" with "translating virtual to physical" → They are different! PFN comes from array index, not from MMU.

