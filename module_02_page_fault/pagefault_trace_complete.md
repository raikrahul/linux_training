# FULL SYSTEM TRACE: The Life of a Page Fault

# 1. TRIGGER: Userspace Write
Location: `mm_exercise_user.c`.
Instruction: `strcpy(ptr, "offset world")`.
State: `ptr` (0x75e52fc77100) is mapped in VMA but has no PTE (Page Table Entry).
Action: CPU attempts store. Hardware MMU checks TLB. Miss. Checks Page Table.
Result: Hardware raises Exception #14 (Page Fault).
Registers Pushed by HW: `SS`, `RSP`, `RFLAGS`, `CS`, `RIP`, `Error Code` (0x6: User|Write).
Register CR2: `0x75e52fc77100` (Fault Address).

# 2. ENTRY: ASM Handler
Location: `arch/x86/entry/entry_64.S`.
Label: `asm_exc_page_fault`.
Work:
-   `PUSH_AND_CLEAR_REGS`. Saves RAX, RBX, RCX... to stack.
-   Stack now matches `struct pt_regs`.
Call: `exc_page_fault(regs, error_code)`.

# 3. KERNEL ENTRY: exc_page_fault
Location: `arch/x86/mm/fault.c`.
Line 1481: `address = read_cr2()`. Value: 0x75e52fc77100.
Line 1523: `handle_page_fault(regs, error_code, address)`.

# 4. DISPATCH: handle_page_fault
Location: `arch/x86/mm/fault.c`.
Line 1467: `do_user_addr_fault(regs, error_code, address)`.
Why: Address is below TASK_SIZE_MAX.

# 5. CONTEXT: do_user_addr_fault
Location: `arch/x86/mm/fault.c`.
Line 1199: Function Entry.
Line 1272: `if (user_mode(regs)) flags |= FAULT_FLAG_USER`. Result: True.
Line 1290: `if (error_code & WRITE) flags |= FAULT_FLAG_WRITE`. Result: True.
Line 1347: `vma = lock_mm_and_find_vma(mm, address, regs)`.
  -   Search Maple Tree `mm->mm_mt`.
  -   Found VMA: 0x75e52fc77000-0x75e52fc7a000.
Line 1375: `handle_mm_fault(vma, address, flags, regs)`.

# 6. ROUTING: handle_mm_fault
Location: `mm/memory.c`.
Line 5519: Function Entry.
Line 5551: `__handle_mm_fault(vma, address, flags)`.

# 7. WALKER: __handle_mm_fault
Location: `mm/memory.c`.
Line 5298: `vmf.address = 0x75...7000`. (Aligned).
Line 5312: `pgd_offset` -> OK.
Line 5317: `pud_alloc`. Allocates 4KB for PUD if missing.
Line 5347: `pmd_alloc`. Allocates 4KB for PMD if missing.
Line 5386: `handle_pte_fault(&vmf)`.

# 8. HANDLER: handle_pte_fault
Location: `mm/memory.c`.
Logic:
-   PMD is present.
-   PTE is NONE (Address is empty in RAM).
-   `vma` is Anonymous.
Call: `do_anonymous_page(&vmf)`.

# 9. ALLOCATOR: do_anonymous_page
Location: `mm/memory.c`.
Line 4279: Check if Read-Only. False (We are writing).
Line 4306: `folio = alloc_anon_folio(vmf)`.
  -   Calls `alloc_pages` (Buddy System).
  -   Returns PFN 0x38944c (Physical RAM).
Line 4326: `mk_pte`. Creates Entry Value.
  -   PFN: 0x38944c.
  -   Flags: Present, User, Write, Dirty, Accessed.
  -   Value: 0x800000038944c867.
Line 4360: `set_ptes`.
  -   Writes 0x800000038944c867 to kernel virtual address of the PTE.
  -   This physically updates the Page Table in RAM.

# 10. REWIND: Return Chain
-   `do_anonymous_page` -> RET 0.
-   `handle_pte_fault` -> RET 0.
-   `__handle_mm_fault` -> RET 0.
-   `handle_mm_fault` -> RET 0.
-   `do_user_addr_fault` -> Done.

# 11. EXIT: ASM Handler
Location: `entry_64.S`.
Work:
-   `POP_REGS`. Restores RAX, RBX...
-   `IRET`. Pop RIP, CS, RFLAGS, RSP, SS.
-   CPU State restored to `strcpy`.

# 12. RESUME: Userspace
Instruction: `strcpy` retries store.
Hardware: Checks TLB/Page Table.
Result: Present! Store succeeds.
Contents of RAM 0x38944c100 becomes "offset world".
