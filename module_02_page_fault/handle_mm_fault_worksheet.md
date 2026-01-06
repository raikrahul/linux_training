# AXIOMATIC DERIVATION: __handle_mm_fault

01. PROBLEM: `handle_mm_fault` is a generic wrapper for architecture-specific code but the logic to allocate pages and fix PTEs lives in a unified helper.
02. AXIOM: `handle_mm_fault` (mm/memory.c) calls `__handle_mm_fault` after verifying the VMA is not for a kernel address.
03. SOURCE: /home/r/Desktop/learn_kernel/source/mm/memory.c:5551 -> `ret = __handle_mm_fault(vma, address, flags);`
04. DEFINITION: `__handle_mm_fault` = Architecture-independent core fault handler.
05. INPUT 1: `vma` (struct vm_area_struct *) -> Descriptor of the memory region.
06. INPUT 2: `address` (unsigned long) -> The virtual address that triggered the fault (CR2 value).
07. INPUT 3: `flags` (unsigned int) -> Bitmask specifying the fault context (WRITE, USER, etc.).
08. AXIOM: The fault context flags are defined in `include/linux/mm.h`.
09. FAULT_FLAG_WRITE = 1 << 0 = 0x01. (Checked: mm.h:508)
10. FAULT_FLAG_MKWRITE = 1 << 1 = 0x02.
11. FAULT_FLAG_ALLOW_RETRY = 1 << 2 = 0x04.
12. FAULT_FLAG_RETRY_NOWAIT = 1 << 3 = 0x08.
13. FAULT_FLAG_KILLABLE = 1 << 4 = 0x10.
14. FAULT_FLAG_TRIED = 1 << 5 = 0x20.
15. FAULT_FLAG_USER = 1 << 6 = 0x40.
16. DERIVATION: A user-space write fault will have `flags & (0x40 | 0x01)` set.
17. CALCULATION: 0x40 | 0x01 = 0x41.
18. DERIVATION: 0x41 = 65 in decimal.
19. AXIOM: `vm_fault` structure is allocated on the stack to store the fault state.
20. DERIVATION: `struct vm_fault vmf = { .vma = vma, .address = address & PAGE_MASK, .real_address = address, .flags = flags, ... }`
21. CALCULATION: Address = 0x794e57539100.
22. PAGE_MASK = ~((1 << 12) - 1) = ~4095 = 0xFFFFFFFFFFFFF000.
23. CALCULATION: vmf.address = 0x794e57539100 & 0xFFFFFFFFFFFFF000 = 0x794e57539000.
24. CALCULATION: vmf.real_address = 0x794e57539100.
25. OFFSET DERIVATION (struct vm_fault):
26. vma: start=0, size=8. Next=8.
27. gfp_mask: start=8, size=4. Next=12.
28. pgoff: start=16 (Must align to 8), size=8. Next=24.
29. address: start=24, size=8. Next=32.
30. real_address: start=32, size=8. Next=40.
31. flags: start=40, size=4. Next=44.
32. pmd: start=48 (Must align to 8), size=8. Next=56.
33. pud: start=56, size=8. Next=64.
34. AXIOM: `__handle_mm_fault` proceeds to walk the page tables from PGD -> P4D -> PUD -> PMD.
35. DERIVATION: If PMD is missing, it calls `__pud_alloc` (indirectly) then `__pmd_alloc`.
36. DERIVATION: If PMD is present but points to no PTE, it calls `handle_pte_fault`.
37. AXIOM: Return type `vm_fault_t` (unsigned int).
38. VM_FAULT_COMPLETED = 0x0000.
39. VM_FAULT_OOM = 0x0001.
40. VM_FAULT_SIGBUS = 0x0002.
41. VM_FAULT_MAJOR = 0x0004.
42. VM_FAULT_WRITE = 0x0008.
43. VM_FAULT_LOCKED = 0x0010.
44. VM_FAULT_RETRY = 0x0400.
45. DERIVATION: A successful anonymous allocation usually returns 0x0000 (No special flag) or VM_FAULT_LOCKED.
46. TEST CASE 1: Read fault on present page. Result -> 0 (No fault).
47. TEST CASE 2: Write fault on anonymous memory (COW). Result -> 0.
48. TEST CASE 3: OOM situation. Result -> 1.
49. CALCULATION: 0x7b73661c7100 >> 30 = 0x1ED (PUD index).
50. CALCULATION: 0x7b73661c7100 >> 21 = 0x7B73 (PMD component).
51. CALCULATION: (0x7b73661c7100 >> 21) & 0x1FF = 0x1B0 (PMD index).
52. CALCULATION: (0x7b73661c7100 >> 12) & 0x1FF = 0x1C7 (PTE index).
53. DERIVATION: kernel must reach PTE index 0x1C7 to fix the fault.
54. AXIOM: `handle_pte_fault` is the next function in the chain.

NEW THINGS INTRODUCED WITHOUT DERIVATION: None.
