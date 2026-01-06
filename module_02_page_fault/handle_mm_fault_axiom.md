:AXIOM 0: THE INPUTS (FROM PREVIOUS STEPS)
01. `vma` = Pointer to Memory Range struct (0x75...) found in tree walk.
02. `address` = Faulting Address (0x75e52fc77100) from CR2 register.
03. `flags` = Derived from HW Error Code (Write=1, User=1).

:AXIOM 1: THE PAGE MASK (MATH)
04. Axiom: Page Size is 4096 bytes (2^12).
05. Math: To align 0x75e52fc77100 to page boundary, clear lower 12 bits.
06. Mask: `PAGE_MASK` = ~0xFFF = 0xFFFFFFFFFFFFF000.
07. Calc: 0x75e52fc77100 & 0xFFFFFFFFFFFFF000 = 0x75e52fc77000.
08. Defines `vmf.address = 0x75e52fc77000`.

:AXIOM 2: THE VM_FAULT STRUCTURE (DATA CONTAINER)
09. Struct `vm_fault` (include/linux/mm.h).
10. Purpose: A suitcase to carry arguments down 4 levels of function calls.
11. Content: `vma`, `address` (aligned), `real_address` (raw), `flags`.
12. Line 5298: Fills this suitcase.

:AXIOM 3: THE MEMORY MAP (CONTEXT)
13. `mm = vma->vm_mm`.
14. `pgd = pgd_offset(mm, address)` (Line 5312).
15. Math: `pgd_index = (address >> 39) & 0x1FF`.
16. Action: `mm->pgd + pgd_index`.
17. Result: Pointer to the Top Level Page Table Entry (PGD).

:AXIOM 4: THE ALLOCATION WALK (ALLOCATION)
18. Goal: We need a PTE (Level 1). We only have PGD (Level 4).
19. Problem: Levels 3 (PUD) and 2 (PMD) might not exist yet (Empty/Null).
20. Line 5313: `p4d_alloc`. (x86_64 4-level: P4D == PGD. No-op).
21. Line 5317: `pud_alloc(mm, p4d, address)`.
    - Check: Is `*p4d` empty?
    - If Yes: Allocate a 4KB Page for PUD Table. Write physical addr to P4D.
    - If No: Return existing PUD address.
22. Line 5347: `pmd_alloc(mm, pud, address)`.
    - Check: Is `*pud` empty?
    - If Yes: Allocate a 4KB Page for PMD Table. Write physical addr to PUD.
23. Result: We now have a valid PMD pointer.

:AXIOM 5: THE FINAL CALL (HANDOFF)
24. We have PMD. We need PTE.
25. Line 5386: `handle_pte_fault(&vmf)`.
26. Trace: Accessing `*pmd` inside this next function will find/alloc the PTE.

:CONCLUSION (PRIMATE SUMMARY)
27. HW gave Address.
28. Math aligned it to Page.
29. Code walked down PGD -> PUD -> PMD.
30. If any level was missing, it allocated a blank page for it.
31. Now ready for the final step (PTE).
