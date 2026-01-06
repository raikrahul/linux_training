# FULL TRACE: handle_mm_fault (The Allocation)

# 1. Call: handle_mm_fault
Caller: `do_user_addr_fault` (fault.c:1375).
Input: 
- `vma`: 0xffff888123 (Structure for VA 0x75...)
- `address`: 0x75e52fc77100 (Faulting Addr)
- `flags`: FAULT_FLAG_WRITE | FAULT_FLAG_USER | FAULT_FLAG_DEFAULT
- `regs`: 0xffffc9000... (Stack Pointer to Regs)
Line 5519: Function Entry.
Line 5528: Call `sanitize_fault_flags`.
  Logic: Check for incompatible flags (e.g. Write on ReadOnly).
  Result: OK (0).
Line 5532: Call `arch_vma_access_permitted`.
  Logic: Check P-Keys / Protection Keys.
  Result: OK (1).
Line 5546: Call `lru_gen_enter_fault(vma)`.
  Work: Mark VMA as "active" for LRU Page Reclaimer.
Line 5548: Check `is_vm_hugetlb_page(vma)`.
  Logic: Is this a HugeTLB (2MB/1GB) specific mapping?
  Data: `vma->vm_flags & VM_HUGETLB` is 0.
  Result: False.
Line 5551: Call `__handle_mm_fault(vma, address, flags)`.
  Note: This is the MAIN worker.

# 2. Call: __handle_mm_fault
Caller: Line 5551.
Input: Same vma, address, flags.
Line 5298: Initialize `struct vm_fault vmf`.
  Value `vmf.address`: 0x75e52fc77000 (Masked with PAGE_MASK).
  Value `vmf.pgoff`: Derived from (Address - VMA->Start).
  Value `vmf.gfp_mask`: Flags for memory allocation (GFP_KERNEL | ...).
Line 5312: `pgd = pgd_offset(mm, address)`.
  Work: Read CR3, find index (Address >> 39).
  Result: PGD Pointer.
Line 5313: `p4d = p4d_alloc(mm, pgd, address)`.
  Work: If P4D not present, allocate page.
  Note: x86_64 4-level paging uses P4D=PGD (Folded).
  Result: P4D Pointer (Same as PGD).
Line 5317: `vmf.pud = pud_alloc(mm, p4d, address)`.
  Work: Read P4D. If empty, allocate PUD page.
  Result: PUD Pointer.
Line 5347: `vmf.pmd = pmd_alloc(mm, vmf.pud, address)`.
  Work: Read PUD. If empty, allocate PMD page.
  Result: PMD Pointer.
Line 5386: Call `handle_pte_fault(&vmf)`.
  Note: We are now at the lowest level (PTE).

# 3. Call: handle_pte_fault (Inferred Location)
Caller: Line 5386.
Work:
1.  Check `pmd_none`. It is NOT none (we just allocated/found it).
2.  Lock Page Table: `spin_lock(vmf->ptl)`.
3.  Read PTE: `vmf->orig_pte = *vmf->pte`.
4.  Value: 0 (Empty/None) because this is a new allocation.
5.  Branch: `if (pte_none(vmf->orig_pte))`.
    Call `do_anonymous_page(&vmf)`.

# 4. Call: do_anonymous_page (The Allocator)
Caller: handle_pte_fault.
Work:
1.  Call `alloc_zeroed_user_highpage_movable`.
    -   Calls `alloc_pages(GFP_HIGHUSER | __GFP_ZERO)`.
    -   Buddy Allocator returns physical page (PFN 0x38944c).
    -   Kernel VA: 0xffff88838944c000.
2.  Memset: Page is zeroed.
3.  Make PTE: `entry = mk_pte(page, vma->vm_page_prot)`.
    -   Sets PRESENT, RW, USER bits.
4.  Call `set_pte_at(mm, address, vmf->pte, entry)`.
    -   Hardware: Writes 0x800000038944c867 to RAM.
5.  Release Lock: `spin_unlock(vmf->ptl)`.
6.  Return: VM_FAULT_COMPLETED.

# 5. Return Chain
- `handle_pte_fault` returns VM_FAULT_COMPLETED.
- `__handle_mm_fault` returns VM_FAULT_COMPLETED.
- `handle_mm_fault` receives status.
Line 5567: `mm_account_fault(mm, regs, address...)`.
  Work: `current->min_flt++`. (Minor Fault).
Line 5569: Return to `do_user_addr_fault`.

# 6. Conclusion
The "magic" was in Step 4:
-   `alloc_pages` got the RAM.
-   `mk_pte` created the bits.
-   `set_pte_at` linked them.
-   Result: Next CPU instruction succeeds.
