:AXIOM 0: THE HARDWARE STATE (Time T0)
01. REGISTER CR2: 0x75e52fc77100 (The address you accessed).
02. REGISTER CR3: 0x1000_phys_user (Your Page Table).
03. REGISTER RIP: 0x55_user_code (Your Instruction).

:TRACE 1: THE ENTRY (Time T0 + 1 cycle)
04. CPU Event: Exception 14 (#PF).
05. Action: Jump to `asm_exc_page_fault` (arch/x86/entry/entry_64.S).
06. Action: Save Registers to Stack (`struct pt_regs`).
07. Call: `exc_page_fault(regs, error_code)`.

:TRACE 2: THE UNKNOWN (THE "MISSING CODE")
08. Function: `do_user_addr_fault` (arch/x86/mm/fault.c).
09. Line A: `address = read_cr2();`
    Value: 0x75e52fc77100.
10. Line B: `mm = current->mm;`
    Value: Your Process Memory Map.
11. Line C: `vma = lock_mm_and_find_vma(mm, address, regs);`
    
:TRACE 3: THE VMA LOOKUP (Time T0 + 500ns)
12. Function: `lock_mm_and_find_vma` (mm/memory.c).
13. Input: Address 0x75e52fc77100.
14. Action: Search the Maple Tree (`mm->mm_mt`).
15. Calculation: Is there a VMA where `vma->vm_start <= address < vma->vm_end`?
16. Result: Found VMA `[0x75e52fc77000 - 0x75e52fc7a000]`.
17. Check: 0x75e52fc77000 <= 0x75e52fc77100 < 0x75e52fc7a000. ✓
18. Return: Pointer to VMA struct.

:TRACE 4: THE HANDLER (Time T0 + 1000ns)
19. Code: `handle_mm_fault(vma, address, flags, regs);`
20. This is where `pagefault_observer.c` intercepted.

:CONCLUSION (YOUR QUESTION)
21. "What does the kernel do?"
22. It reads CR2.
23. It uses `current->mm` to find the Tree.
24. It walks the Tree to find the VMA that "owns" CR2.
25. It passes that VMA to `handle_mm_fault`.
