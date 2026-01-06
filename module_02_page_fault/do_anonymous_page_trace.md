# TRACE: do_anonymous_page (The Zero Memory Allocator)

# 1. CALL: do_anonymous_page(vmf)
Caller: `handle_pte_fault` (mm/memory.c).
Input:
- `vmf`: Struct with Address 0x75e52fc77000.
- `vmf->flags`: Contains FAULT_FLAG_WRITE (We are writing).
- `vma->vm_flags`: VM_WRITE | VM_READ (User permissions).
Line 4254: Function Entry.
Line 4268: `if (vma->vm_flags & VM_SHARED)`.
  Data: `VM_SHARED` is 0 (Private Mapping).
  Result: False.

# 2. CHECK: pte_alloc (Page Table Existence)
Line 4275: `if (pte_alloc(vma->vm_mm, vmf->pmd))`.
  Work: Check if PMD points to valid Page Table Page.
  Real Data: We previously allocated PMD in `__handle_mm_fault`, so this usually returns 0 (Success).
  Result: 0.

# 3. DECISION: Zero Page vs Real Page
Line 4279: `if (!(vmf->flags & FAULT_FLAG_WRITE)...)`.
  Logic: If READ fault, map to global "Zero Page" (Read-Only).
  Real Data: `vmf->flags` HAS `FAULT_FLAG_WRITE` (We did `strcpy`).
  Result: Condition False. We need a REAL Writeable Page.

# 4. PREPARATION: Anon VMA (Reverse Map)
Line 4303: `if (unlikely(anon_vma_prepare(vma)))`.
  Work: Ensure VMA is attached to `anon_vma` structure for Reverse Mapping (swapping).
  Result: 0 (Success).

# 5. ALLOCATION: The Physical RAM
Line 4306: `folio = alloc_anon_folio(vmf)`.
  Caller: Calls `alloc_pages` (Buddy Allocator).
  Work: Find free 4KB physical page.
  Error Check: If RAM full, returns NULL -> OOM.
  Real Data: Returns valid struct folio* (wrapper for struct page).
  Value: PFN 0x38944c (Derived from our previous inspection).
  Result: Success.

# 6. ACCOUNTING: MemCgroup (Control Groups)
Line 4315: `if (mem_cgroup_charge(folio...))`.
  Work: Check if your process exceeded Docker/Container limits.
  Result: 0 (Charged successfully).

# 7. LOGIC: Make PTE Entry (The Bits)
Line 4326: `entry = mk_pte(&folio->page, vma->vm_page_prot)`.
  Work: Combine Physical Address (PFN 0x38944c) + Protections.
Line 4327: `entry = pte_sw_mkyoung(entry)`.
  Work: Set ACCESSED bit (Software tracking).
Line 4328: `if (vma->vm_flags & VM_WRITE)`.
  Data: True.
Line 4329: `entry = pte_mkwrite(pte_mkdirty(entry), vma)`.
  Work: Set RW bit + DIRTY bit.
  Result: `entry` = 0x800000038944c867 (Present, RW, User, Dirty, Accessed, NoExec).

# 8. LOCK: Critical Section
Line 4331: `vmf->pte = pte_offset_map_lock(..., &vmf->ptl)`.
  Work: Lock the specific Page Table (Spinlock).
  Why: Prevent other threads from modifying this PTE same time.
  Result: Locked.

# 9. INSERTION: Link to VMA
Line 4355: `folio_add_new_anon_rmap(folio, vma, addr)`.
  Work: Tell Reverse Map "This Page belongs to this VMA".
Line 4356: `folio_add_lru_vma(folio, vma)`.
  Work: Add to LRU List (for swapping later).

# 10. WRITE: Update Hardware
Line 4360: `set_ptes(vma->vm_mm, addr, vmf->pte, entry, 1)`.
  Work: Writes 64-bit `entry` to memory address of PTE.
  Real Action: This is the moment the mapping becomes "Real" to hardware.

# 11. UNLOCK & RETURN
Line 4366: `pte_unmap_unlock`.
Line 4367: `return ret` (0).
