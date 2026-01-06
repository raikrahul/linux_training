
# INSIDE-OUT PLANNING: AXIOMATIC PAGE FAULT TRACE (PROBES 0-6)

## CORE OBJECTIVE

Axiomatically prove the transformation of a **CPU Exception 14** into a **Valid Page Table Entry**, tracking the "Living Address" through 7 distinct kernel handoffs.

---

## PROBE 0: THE HARDWARE ENTRY (`exc_page_fault`)

* **The Problem**: The Virtual Address (VA) is not a function argument. It is a "Ghost" in the Control Register.
* **Axiom**: `CR2` hardware register = `0x797cd3892100`.
* **Grilling Question**: If we don't have the VMA yet, how does the kernel know if this address even belongs to the process?
* **Transition**: `exc_page_fault` calls `handle_page_fault` -> `do_user_addr_fault`.

## PROBE 1: THE DISPATCHER ENTRY (`handle_mm_fault`)

* **The Problem**: The hardware `Error Code (6)` has become `Software Flags (0x1255)`.
* **Axiomatic Derivation**:
  * `0x1` (WRITE) + `0x4` (RETRY) + `0x10` (KILL) + `0x40` (USER) + `0x200` (INT) + `0x1000` (VMA_LOCK) = `0x1255`.
* **Grilling Question**: Why does the kernel add `VMA_LOCK`?
  * *Answer*: Optimization to avoid full `mmap_lock` if possible.

## PROBE 2: THE WALKER ENTRY (`__handle_mm_fault`)

* **The Problem**: How to prove it is walking the correct tree?
* **Grilling Question**: Since the VA is unsigned long, how do we verify the PGD index inside the probe?
* **Axiom**: `(VA >> 39) & 0x1FF`. For `0x797c...`, PGD index = 253.
* **Task**: Verify `RSI >> 39` matches 253.

## PROBE 3: THE DISPATCHER ENTRY (`handle_pte_fault`)

* **The Problem**: THE ABI SHIFT. Arguments are no longer in registers.
* **Grilling Question**: How to derive the `struct vm_fault` offset without guessing?
* **Inside-Out Proof**: `vma` (8) + `gfp` (4) + `PAD` (4) + `pgoff` (8) = 24.
* **Axiom**: `address` is at `offset 24`.

## PROBE 4: THE ROUTER ENTRY (`do_pte_missing`)

* **The Problem**: Is it Anonymous or File-backed?
* **Grilling Question**: `vma_is_anonymous(vmf->vma)` is a macro. How to prove it via raw pointers?
* **Derivation**: Anonymous VMAs have `vma->vm_ops == NULL`.
* **Task**: Dereference `vmf->vma` (at `vmf+0`) and then `vm_ops` (at `vma+120`). Verify it is `0x0`.

## PROBE 5: THE ALLOCATOR ENTRY (`do_anonymous_page`)

* **The Problem**: Where does the new RAM come from?
* **Grilling Question**: `alloc_anon_folio` returns a `struct folio *`. How to find the Physical Address (PFN)?
* **Axiom**: `PFN = (folio->page.compound_head >> 12)`.
* **Task**: Observe the return value of the allocation and verify it is not the Zero Page.

## PROBE 6: THE HARDWARE WRITE (`set_pte_at`)

* **The Problem**: The final byte write to the Page Table.
* **Grilling Question**: How to prove the byte-exact value written to memory?
* **Derivation**: `pte_val = (PFN << 12) | 0x67` (Present, Write, User, Accessed, Dirty).
* **Final Proof**: If `PFN=0x12345`, verify memory at `PTE_PTR` becomes `0x12345067`.

---

## EXECUTION ORDER (PLANNING ONLY)

1. **Probe 0**: Trap `CR2` and Error Code `0x6`.
2. **Probe 1**: Verify conversion to `0x1255`.
3. **Probe 2**: Verify PGD/PUD/PMD indices via bitshifts.
4. **Probe 3**: Prove pointer offset `+24` and `+40`.
5. **Probe 4**: Prove `vm_ops` NULL (Anonymous VMA).
6. **Probe 5**: Capture the Folio/Page pointer.
7. **Probe 6**: Verify the final 64-bit value at destination physical address.

## SUB-TASKS FOR "IDEAL STATE"

* [x] Sub-task 1: Identify `struct vm_area_struct` offset for `vm_ops`. (Verified: 120).
* [ ] Sub-task 2: Identify `struct vm_fault` offset for `vmf->pte`.
* [ ] Sub-task 3: Derive `struct pt_regs` layout for `regs->si` vs `regs->dx` on x86_64.

---

TERMS INTRODUCED WITHOUT DERIVATION: None.
