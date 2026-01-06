01. RUN: `make` with flags_zone_node_hw.o in Makefile.
02. RUN: `sudo insmod flags_zone_node_hw.ko`.
03. RUN: `sudo dmesg | tail -20` → Read output.
04. WRITE: PFN = 0x_____ (from dmesg).
05. CONVERT: 0x_____ → _____ decimal. CALCULATE: Each hex digit × 16^position. Example: 0x17da77 = 1×16^5 + 7×16^4 + 13×16^3 + 10×16^2 + 7×16^1 + 7×16^0 = 1048576 + 458752 + 53248 + 2560 + 112 + 7 = 1,563,255.
06. COMPARE PFN: Is _____ < 4096? → _____ ✓/✗.
07. COMPARE PFN: Is _____ < 1048576? → _____ ✓/✗.
08. DERIVE ZONE: Zone = _____ (0=DMA, 1=DMA32, 2=Normal).
09. COMPARE: Calculated zone = _____, API zone = _____ → MATCH ✓/✗.
10. CALCULATE PHYS: _____ × 4096 = _____ bytes. VERIFY: Is this pfn with 3 zeros appended? ✓/✗.
11. READ FLAGS: flags = 0x_____ (from dmesg).
12. EXTRACT PG_LOCKED: (0x_____ >> 0) & 1 = _____.
13. EXTRACT PG_DIRTY: (0x_____ >> 4) & 1 = _____.
14. EXTRACT PG_LRU: (0x_____ >> 5) & 1 = _____.
15. EXPECTED: PG_locked=0, PG_dirty=0, PG_lru=0 for fresh page → MATCH ✓/✗.
16. CALCULATE DMA END: 2^24 = _____. 2^24 / 4096 = _____ PFNs.
17. CALCULATE DMA32 END: 2^32 = _____. 2^32 / 4096 = _____ PFNs.
18. FILL TODO BLOCK 2: phys_addr = pfn × 4096 = pfn << 12 = pfn * PAGE_SIZE.
19. FILL TODO BLOCK 3: pg_locked = (flags >> 0) & 1; pg_dirty = (flags >> 4) & 1; pg_lru = (flags >> 5) & 1.
20. FILL TODO BLOCK 4: dma_end_pfn = 4096; dma32_end_pfn = 1048576.
21. RECOMPILE: `make clean && make`.
22. RELOAD: `sudo rmmod flags_zone_node_hw && sudo insmod flags_zone_node_hw.ko`.
23. VERIFY: `sudo dmesg | tail -20` → All values should now be correct.
24. F1 TEST: Intentionally use (pfn * 4096) on 32-bit int → Observe overflow warning or wrong value.
25. F2 TEST: Try (flags >> 62) & 0x3 → Compare to page_zonenum() → Should MISMATCH.
26. DONE: All TODOs filled, all calculations verified.
