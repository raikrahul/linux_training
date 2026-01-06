# GRILLING THE TASK: DECODING MOVABLE AND KSM

## PHASE 1: DISSECTING THE PROBLEM STATEMENT

### 1. PHRASE: "If the page does not appear on LRU lists (termed ‘non-LRU’)"
*   **HINT:** "non-LRU".
*   **LINE OF ATTACK 1:** What pages are non-LRU?
    *   Kernel slab pages? (Usually have `page->slab_cache`).
    *   Driver pages? (Device memory `ZONE_DEVICE`).
    *   Balloon pages? (Used by VirtIO for memory reclaiming).
    *   **Movable** pages?
*   **LINE OF ATTACK 2:** How to create a "non-LRU" page in userspace?
    *   Normal `mmap` pages ARE on LRU (Active/Inactive).
    *   We need something special.
    *   Maybe `mmap` with a specific driver? Or `madvise`?

### 2. PHRASE: "it may set the PAGE_MAPPING_MOVABLE flag"
*   **HINT:** `PAGE_MAPPING_MOVABLE` = `0x2` (Bit 1 set).
*   **LINE OF ATTACK 1:** Check if `0x2` is mutually exclusive with `0x1`?
    *   Text says "PAGE_MAPPING_KSM" is a "combination".
    *   So "MOVABLE alone" means Bits `10`.
*   **LINE OF ATTACK 2:** Verify Pointer Alignment.
    *   If Bit 1 is used, pointer must be aligned to at least 4 bytes (0x...0, 0x...4).
    *   We proved 8-byte alignment, so Bits `000` are free. Bit 1 is safe.

### 3. PHRASE: "points at a struct movable_operations object"
*   **HINT:** `struct movable_operations`.
*   **LINE OF ATTACK 1:** Find this struct definition.
    *   It's likely a table of function pointers (like `vtable` or `file_operations`).
    *   Used for migration/compaction?
    *   **SEARCH:** `grep -r "struct movable_operations" /usr/src/linux-headers...`

### 4. PHRASE: "Finally, if the flags are set to PAGE_MAPPING_KSM (a combination of the prior two)"
*   **HINT:** "Combination" -> `0x1 | 0x2 = 0x3`.
*   **LINE OF ATTACK 1:** Bits `11`.
*   **LINE OF ATTACK 2:** This consumes BOTH low bits.
*   **EDGE CASE:** Is it possible to have `0x3` without being KSM? No, definition is explicit.

### 5. PHRASE: "points to a private KSM data structure"
*   **HINT:** `private KSM data structure`.
*   **LINE OF ATTACK 1:** What struct is this? `struct ksm_stable_node`? `struct ksm_rmap_item`?
*   **LINE OF ATTACK 2:** `private` implies it might not be in public headers.
    *   Failed to find `struct ksm_stable_node` in headers previously.
    *   It might be defined inside `mm/ksm.c` (opaque to other modules).
    *   **TRAP:** If we cannot see the struct definition, we cannot cast to it or print its members. We can ONLY print the address.

### 6. PHRASE: "allows for memory pages to be de-duplicated"
*   **HINT:** "De-duplicated".
*   **LINE OF ATTACK 1:** Triggering KSM.
    *   Process A: Allocates Page X (Content: All Zeros).
    *   Process B: Allocates Page Y (Content: All Zeros).
    *   `madvise(addr, len, MADV_MERGEABLE)`.
    *   Wait for `ksmd` kernel thread.
    *   Result: `VA_A` and `VA_B` point to Same PFN.
    *   Check `page->mapping`: Should end in `11` (0x3).

---

## PHASE 2: PLANNING THE EXPERIMENT (KSM FOCUS)

**OBJECTIVE:** We successfully did State 00 (File) and 01 (Anon). Now we want State 11 (KSM).
*State 10 (Movable) is hard to trigger from userspace without specific drivers, so we focus on KSM.*

### 1. USERSPACE TRIGGER (`mapping_user.c` Extension)
*   **Step 1:** Allocate a page. `mmap(ANON, PRIVATE)`.
*   **Step 2:** Fill with predictable content (e.g. `memset(0xAA)`).
*   **Step 3:** Tell kernel to merge. `madvise(ptr, size, MADV_MERGEABLE)`.
*   **Step 4:** Wait. KSM is lazy.
*   **Step 5:** How to *force* merge?
    *   Maybe create TWO pages with SAME content and mark BOTH mergeable.
    *   Sleep inside the C program? Or wait for user input?

### 2. KERNEL INSPECTION (`mapping_hw.c` Extension)
*   **Step 1:** Add logic for Case 3 (`11`).
*   **Step 2:** Decode pointer: `val & ~3`.
*   **Step 3:** Type interpretation: `(struct ksm_stable_node *)`.
    *   **ISSUE:** Definition missing.
    *   **WORKAROUND:** Use `void *` or just print hex address. Do not dereference.

### 3. AXIOMATIC CHECKS
*   **Check 1:** Does `MADV_MERGEABLE` guarantee KSM?
    *   No. `ksmd` might be disabled (`/sys/kernel/mm/ksm/run`).
    *   We must check/enable it in shell script.
*   **Check 2:** Does KSM *always* set `PAGE_MAPPING_KSM`?
    *   Yes, for stable nodes (shared pages).
    *   Unstable nodes might still be Anon?

## PHASE 3: EXECUTION REPORT STRUCTURE
1.  **Check KSM Config:** `cat /sys/kernel/mm/ksm/run`.
2.  **Enable KSM:** `echo 1 > ...`.
3.  **Run Trigger:** Create duplicates.
4.  **Inspect:** See if flags change from `01` (Anon) to `11` (KSM).

