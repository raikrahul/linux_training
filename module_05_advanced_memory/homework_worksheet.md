01. RUN: `cd /home/r/Desktop/learnlinux/kernel_exercises/metadata_union && make`.
02. RUN: `sudo insmod metadata_union_hw.ko`.
03. RUN: `sudo dmesg | tail -25` → Read output.

=== TODO BLOCK 1 ===
04. WRITE: lru.prev from dmesg = 0x___.
05. WRITE: lru.next from dmesg = 0x___.
06. FILL: `offset_8_15_normal = (unsigned long)normal_page->lru.prev;`
07. FILL: `offset_16_23_normal = (unsigned long)normal_page->lru.next;`

=== TODO BLOCK 2 ===
08. FILL: `is_slab = PageSlab(normal_page);`
09. VERIFY: is_slab = ___ → Expected 0 ✓/✗.

=== TODO BLOCK 3 ===
10. FILL: `slab_page = virt_to_page(slab_obj);`
11. UNDERSTAND: slab_obj = 0xffff... (object pointer). slab_page = 0xfffff... (struct page pointer). DIFFERENT!
12. CALCULATE: slab_obj is 128 bytes inside a 4096-byte page. Many objects per page.

=== TODO BLOCK 4 ===
13. FILL: `is_slab = PageSlab(slab_page);`
14. VERIFY: is_slab = ___ → Expected 1 ✓/✗.

=== TODO BLOCK 5 ===
15. FILL: `offset_8_15_slab = (unsigned long)slab_page->lru.prev;`
16. FILL: `offset_16_23_slab = (unsigned long)slab_page->lru.next;`
17. WRITE: slab offset 16-23 = 0x___.
18. COMPARE: Normal offset 16-23 = 0x___ vs Slab offset 16-23 = 0x___ → DIFFERENT? ___

=== TODO BLOCK 6 ===
19. VERIFY: offset_16_23_normal != offset_16_23_slab → ___ ✓/✗.
20. INTERPRET: Normal at offset 16-23 = lru.next (list pointer, NULL for fresh page).
21. INTERPRET: Slab at offset 16-23 = freelist (pointer to first free object in slab).
22. CONCLUSION: Same 40 bytes, different meaning based on PageSlab() flag.

=== RECOMPILE AND VERIFY ===
23. RUN: `make clean && make`.
24. RUN: `sudo rmmod metadata_union_hw && sudo insmod metadata_union_hw.ko`.
25. RUN: `sudo dmesg | tail -25` → All TODO values should now be filled.

=== FAILURE PREDICTIONS ===
26. F01: Forget virt_to_page() → slab_obj is NOT struct page*, must convert.
27. F02: Access page->mapping on slab page → Wrong interpretation, crash risk.
28. F03: Assume all pages have lru list linked → Fresh pages have poison/NULL.
29. F04: Confuse 128-byte object with 4096-byte page → Object is INSIDE page.
30. F05: Forget PageSlab() check → Must check before interpreting union fields.
