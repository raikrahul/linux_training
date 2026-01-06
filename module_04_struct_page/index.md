---
layout: post
title: "Mapping Field Decode Exercise"
---
# MAPPING FIELD DECODE EXERCISE

## PHASE 1: AXIOMATIC INTERROGATION (COMPLETED)
We questioned the hardware reality of "2 bits available". We verified it with `axiom_check.c`.

### DATA FROM MACHINE:
- `alignof(struct address_space)` = **8 bytes**
- `alignof(struct anon_vma)` = **8 bytes**
- Bits available = `log2(8)` = **3 bits** (Bits 0, 1, 2 are ALWAYS zero in a valid pointer)
- Flags needed = **2 bits** (PAGE_MAPPING_ANON=1, PAGE_MAPPING_MOVABLE=2)
- Conclusion: **3 bits > 2 bits**. The encoding is physically possible on this machine.

### FLAG DEFINITIONS (FROM SOURCE/CHECK):
- `PAGE_MAPPING_ANON` = `0x1` (Bit 0 set)
- `PAGE_MAPPING_MOVABLE` = `0x2` (Bit 1 set)
- `PAGE_MAPPING_KSM` = `0x3` (Bits 0 and 1 set)

### STATE TABLE DERIVATION:
| Raw Bits [1:0] | Flag Name | Meaning | Target Struct Type |
|----------------|-----------|---------|--------------------|
| `00` | (None) | **Page Cache** | `struct address_space *` |
| `01` | `PAGE_MAPPING_ANON` | **Anonymous** | `struct anon_vma *` |
| `10` | `PAGE_MAPPING_MOVABLE` | **Movable** | `struct movable_operations *` |
| `11` | `PAGE_MAPPING_KSM` | **KSM** | `struct ksm_stable_node *`? (Check source) |

---

## PHASE 2: EXECUTION PLAN

### GOAL
To observe `page->mapping` raw value changing states based on how we allocate memory.

### USERSPACE TRIGGERS (`mapping_user.c`)
We need to generate 2 (or 3) distinct types of pages within one process.

1.  **FILE-BACKED PAGE (State 00)**
    *   Action: `mmap()` a file on disk (e.g., create a temp file, write data, map it).
    *   Expected: `page->mapping` lower bits == `00`.
    *   Expected: `page->mapping` points to `inode->i_mapping`.

2.  **ANONYMOUS PAGE (State 01)**
    *   Action: `mmap(MAP_ANONYMOUS)` or `malloc()`.
    *   Expected: `page->mapping` lower bits == `01`.
    *   Expected: `page->mapping & ~3` points to a `struct anon_vma`.

3.  **KSM PAGE (State 11) - HARD PROBABILITY**
    *   Action: Alloc two pages with same content, `madvise(MADV_MERGEABLE)`. All 0s is best candidate.
    *   Requirement: KSM daemon (`ksmd`) must satisfy the merge.
    *   Wait time: Indeterminate. Might not happen in 5 seconds.
    *   Strategy: Try it, but don't fail if it doesn't work. Just observe.

### KERNEL INSPECTOR (`mapping_hw.c`)
Logic to decode the field safely:
1.  Read `unsigned long raw_val = (unsigned long)page->mapping;`
2.  Extract low bits: `long flags = raw_val & 3;`
3.  Extract pointer: `unsigned long ptr = raw_val & ~3UL;`
4.  Switch on `flags`:
    *   Case 0: Print "FILE". Print `ptr`.
    *   Case 1: Print "ANON". Print `ptr`.
    *   Case 2: Print "MOVABLE".
    *   Case 3: Print "KSM".

### SAFETY CHECK
**CRITICAL:** Do NOT dereference the pointer in the module yet. We just want to see the address and bits. Determining *if* it is a valid pointer is Step 3.

---

## NEXT STEPS
1.  Write `mapping_user.c` to create File + Anon mappings.
2.  Write `mapping_hw.c` to inspect them.
---

## PHASE 3: NUMERIC DERIVATION OF THE "IMPOSSIBLE" POINTER

### Q: How can a pointer (page->mapping) hold a flag (0x1) and still be a pointer?

**1. WHAT: The 3-Bit Gap**
*   **Struct Alignment**: 8 bytes (on 64-bit).
*   **Address Rule**: Any address pointing to this struct MUST be a multiple of 8.
*   **Valid Addresses**:
    *   `0x1000` (4096 / 8 = 512.0) ✓
    *   `0x1008` (4104 / 8 = 513.0) ✓
    *   `0x1010` (4112 / 8 = 514.0) ✓
*   **Invalid Addresses**:
    *   `0x1001` (4097 / 8 = 512.125) ✗
    *   `0x1002` (4098 / 8 = 512.250) ✗
    *   `0x1003` (4099 / 8 = 512.375) ✗
    *   ...
    *   `0x1007` (4103 / 8 = 512.875) ✗
*   **Gap Size**: 7 values (1 to 7) between every valid pointer.
*   **Binary View**:
    *   `0x1000` = `...100000000000` (Ends in 000)
    *   `0x1008` = `...100000001000` (Ends in 000)
    *   ∴ Bottom 3 bits are ALWAYS 0 for clear pointers.

**2. WHY: Efficiency (Space Saving)**
*   **Without Trick**: `struct page { void *mapping; int is_anon; };`
    *   Size: 8 bytes (ptr) + 4 bytes (int) + 4 bytes (padding) = 16 bytes.
*   **With Trick**: `struct page { unsigned long mapping_val; };`
    *   Size: 8 bytes.
    *   Encoding: `mapping_val = (anon_vma_ptr | 1)`.
*   **Benefit**: Saves 8 bytes per page.
*   **Scale**: 16GB RAM = 4,194,304 pages.
*   **Total Savings**: 4,194,304 * 8 bytes = **32 MB of RAM saved**.

**3. WHERE: Between the Bits**
*   **Pointer**: `0xffff8880abcd1000`
*   **Flag**: `0x1` (PAGE_MAPPING_ANON)
*   **Operation**: `OR`
    ```
      1111...1000  (Pointer)
    | 0000...0001  (Flag)
    = 1111...1001  (Stored Value)
    ```

**4. WHO: The Kernel (mm/rmap.c)**
*   **Setting**: `page->mapping = (struct address_space *) ((unsigned long)anon_vma | PAGE_MAPPING_ANON);`
*   **Reading**: `struct anon_vma *av = (struct anon_vma *) (page->mapping & ~PAGE_MAPPING_ANON);`

**5. WHEN: Page Fault Time**
*   User calls `mmap(MAP_ANONYMOUS)`. VMA created.
*   User writes data. Page Fault.
*   Kernel allocates page.
*   Kernel creates `anon_vma`.
*   Kernel ENCODES the pointer + flag. Store in `page->mapping`.

**6. WITHOUT: Crash**
*   If you access `page->mapping->host` directly on an ANON page:
    *   Address: `0xffff8880abcd1001`
    *   CPU fetches 8 bytes.
    *   If Hardware enforces alignment: **#AC Exception (Alignment Check)**.
    *   If Hardware allows unaligned: reads bytes from `1001` to `1009`. Garbage data.
*   **MUST DECODE FIRST**.

**7. WHICH: The Least Significant Bit (LSB)**
*   Bit 0 = 1 -> Anonymous Page.
*   Bit 1 = 1 -> Movable Page.
*   Bit 0 & 1 = 1 -> KSM Page.
*   Bit 0 & 1 = 0 -> Page Cache (File).

### CONCRETE EXAMPLE (FROM YOUR SESSION)

```
PID: 219766
ANONYMOUS VA: 0x746d87c00000
RAW MAPPING VALUE: 0xffff888102a9cd21

DECODING STEP-BY-STEP:
1.  Value = 0xffff888102a9cd21
    Binary (last 4 bits): 0001

2.  Extract Flags:
    Mask: 0x3 (0011)
    Calculation: 0001 & 0011 = 0001
    Result: 1 (PAGE_MAPPING_ANON)
    Meaning: This page is backed by anon_vma, NOT a file.

3.  Extract Pointer:
    Mask: ~0x3 (1100)
    Calculation: 0001 & 1100 = 0000
    Last 4 bits become: ...d20 (was ...d21)
    Full Pointer: 0xffff888102a9cd20
    
4.  Verification:
    Is 0xffff888102a9cd20 divisible by 8?
    0x...20 = 32 (decimal). 32 / 8 = 4.
    YES. It is a valid aligned struct pointer.
```

---

## PHASE 4: STRICT AXIOMATIC DERIVATION (RE-DERIVATION)

**AXIOM 1:** Computers address memory by Bytes.
**AXIOM 2:** A Pointer is a Number representing a Byte Address (`0, 1, 2...`).
**AXIOM 3:** The CPU fetches Data in Chunks, not Bytes (Architecture Dependent).
**AXIOM 4:** x86_64 CPU fetches 64-bit Words (8 Bytes) at a time.
**AXIOM 5:** For Efficiency (Alignment), 8-Byte Data must start at addresses divisible by 8.

### DERIVATION 1: THE INVISIBLE ZEROS
1.  **Objective:** Store `struct page` pointer.
2.  **Size:** `sizeof(struct page)` = 64 bytes (on typical build).
3.  **Align:** `alignof(struct page)` = 8 bytes (Word Size).
4.  **Math:** Address `A` must satisfy `A % 8 == 0`.
5.  **Binary:**
    *   `8` (decimal) = `1000` (binary).
    *   Multiples of 8: `0, 8, 16, 24...`
    *   Binary: `0000`, `1000`, `10000`, `11000`...
    *   **Observation:** The last 3 bits are ALWAYS `000`.
6.  **Conclusion:** In any valid kernel pointer to `struct page`, bits [2:0] are mathematically guaranteed to be ZERO.
7.  **Resource:** We have 3 "Bits of Nothing" stored in every pointer.
    *   `P = P_real + 0`.
    *   Therefore `P | 7` changes bits [2:0] without destroying `P_real` (if we mask later).

### DERIVATION 2: THE ENCODING
1.  **Input:**
    *   Pointer `P` (e.g., `0x1000`). Binary: `...10000_000`.
    *   Flag `F` (e.g., `0x1`). Binary: `001`.
2.  **Constraint:** `F < 8` (must fit in 3 bits).
3.  **Operation (Encode):** `E = P | F`.
    *   `...000` OR `...001` = `...001`.
    *   `E = 0x1001`.
4.  **Verification:** `E` is NOT a valid pointer (Odd address).
5.  **Use Case:** We can pass `E` around as a value (unsigned long), but CANNOT dereference it directly.

### DERIVATION 3: THE DECODING
1.  **Input:** Encoded Value `E = 0x1001`.
2.  **Objective 1:** Get Flag `F`.
    *   Mask: `M_flag = 7` (Binary `111`).
    *   Calc: `E & M_flag`.
    *   `0x1001 & 0x7` -> `0001 & 0111` -> `0001` (`1`, Correct).
3.  **Objective 2:** Get Pointer `P`.
    *   Mask: `M_ptr = ~7` (Binary `...11111000`).
    *   Calc: `E & M_ptr`.
    *   `0x1001 & ~0x7` -> `0x1001 & ...111000`.
    *   Binary:
        ```
          ...10000_001
        & ...11111_000
        = ...10000_000 (0x1000, Correct)
        ```
    
### DERIVATION 4: THE KERNEL MAPPING FIELD
1.  **Field:** `page->mapping`.
2.  **Requirement:** Must point to `struct address_space` (Aligned 8).
3.  **Requirement:** Must ALSO track if page is Anonymous.
4.  **Conflict:** Adding `bool is_anon` adds 4-8 bytes to `struct page`.
5.  **Cost:** 4M pages * 8 bytes = 32MB waste.
6.  **Solution:** Use **Derivation 1**.
7.  **Implementation:**
    *   If `page->mapping` ends in `00` -> It is a Pointer.
    *   If `page->mapping` ends in `01` -> It is an Encoded Value (`Anon Ptr | 1`).
    *   If `page->mapping` ends in `10` -> It is an Encoded Value (`Movable Ptr | 2`).
    *   If `page->mapping` ends in `11` -> It is an Encoded Value (`KSM Ptr | 3`).

### DERIVATION 5: THE "HOWEVER" EXPLAINED
*   **Statement:** "However the lower 2 bits are given over to flags..."
*   **Axiom:** Pointers to `align(4)` or `align(8)` objects have 2 or 3 spare bits.
*   **Derivation:** "Given over" means the Kernel Developer decided to use the **Resource** (Derivation 1, Step 7).
*   **Implication:** The field is no longer a pure pointer. It is a **Tagged Union**.
    *   Type A: `struct address_space *` (Tag: 00).
    *   Type B: `struct anon_vma *` (Tag: 01).
*   **Result:** You must check the Tag (Bits) before casting the Pointer.

### NUMERICAL EXAMPLE SET (7 CASES)
**Goal:** Calculate `P | F` and `(P|F) & ~M`.

| Reference `P` | Alignment | Valid? | Flag `F` | Encoded `E` | Decoded `P'` |
|---------------|-----------|--------|----------|-------------|--------------|
| `0x100` | 8 | ✓ | 1 | `0x101` | `0x100` |
| `0x104` | 8 | ✗ | 1 | `0x105` | `0x100` (Wrong!) |
| `0x2000` | 4 | ✓ | 2 | `0x2002` | `0x2000` |
| `0x2000` | 4 | ✓ | 3 | `0x2003` | `0x2000` |
| `0xFFFF` | 2 | ✗ (Odd) | 1 | `0xFFFF` | `0xFFFE` (Wrong!) |
| `0x80` | 16 | ✓ | 7 | `0x87` | `0x80` |
| `0x80` | 16 | ✓ | 8 | `0x88` | **COLLISION!** |

**Surprise in Case 2:** If the original pointer wasn't aligned to the mask size, decoding destroys data. (0x104 -> 0x100).
**Surprise in Case 7:** If the Flag is larger than the alignment gap (8 >= 8), it corrupts the pointer's actual bits. `0x80 | 8 = 0x88`. Decoding `0x88 & ~7` = `0x88`. The flag has merged into the address!
**Therefore:** Flags MUST be strictly smaller than Alignment (`F < Align`).

---

## PHASE 5: EXECUTION TRACE & FINAL PROOF (AXIOMATIC LOG)

**AXIOM 1:** A Theory is valid if and only if Real Data matches Prediction.
**AXIOM 2:** `dmesg` implies Kernel Truth.

### STEP 1: TRIGGER GENERATION (USERSPACE)
1.  **Syscall:** `open("temp_map_file", ...)` -> Returns FD 3.
2.  **Syscall:** `write(3, "F"..., 4096)` -> Extends file to 4096 bytes.
3.  **Syscall:** `mmap(..., MAP_SHARED, fd=3)`
    *   **Action:** Kernel links VMA to File Inode.
    *   **State:** Page Cache Mode.
    *   **Prediction:** `page->mapping` lower bits must be `00`.
4.  **Syscall:** `mmap(..., MAP_ANONYMOUS|MAP_PRIVATE, ...)`
    *   **Action:** Kernel links VMA to `anon_vma`.
    *   **State:** Anonymous Mode.
    *   **Prediction:** `page->mapping` lower bits must be `01`.

### STEP 2: KERNEL INSPECTION (LIVE DATA)
We ran the driver against PID 219766 (captured live).

**TRACE A: FILE PAGE**
1.  **Input:** VA = `0x746d87e07000`.
2.  **Kernel OP:** `get_user_pages(...)` -> Returns `struct page *`.
3.  **Read Raw:** `0xffff88810672e380`.
4.  **Calc Flag:** `0x...380 AND 3`.
    *   Binary `...10000000` & `00000011`.
    *   Result: `0`.
5.  **Calc Ptr:** `0x...380 AND ~3`.
    *   Result: `0xffff88810672e380` (Unchanged).
6.  **Verify:** `0x...380 % 8 == 0`. Aligned ✓.
7.  **Conclusion:** Valid Pointer to `struct address_space`.

**TRACE B: ANON PAGE AXIOMATIC RE-DERIVATION**
01. **Input State**: User calls `mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)`.
    *   *Check: New Variable? No. MAP_ANONYMOUS derived in Phase 3.*
    *   *Check: Jump ahead? No. System call is the start.*
02. **Kernel Value Definition**: `MAP_ANONYMOUS` is defined as `0x20` (32) in `include/uapi/asm-generic/mman-common.h`.
    *   *Check: New Value? Yes, Axiom Source Provided (Header).*
    *   *Check: Calculation? Constant lookup.*
03. **Kernel Logic**: `sys_mmap` sees `flags & MAP_ANONYMOUS` is True (0x22 & 0x20 = 0x20).
    *   *Check: New Calc? derived from 01 and 02.*
    *   *Check: Jump ahead? No. Linear flow.*
04. **Control Flow**: Kernel branches to `mm/mmap.c` -> `vma->vm_ops = NULL` (No file operations).
    *   *Check: New Inference? Derived from logic: No File = No Ops.*
05. **Page Fault Trigger**: User writes `anon_ptr[0] = 'A'`.
    *   *Check: New Event? Just user code execution.*
06. **Page Fault Logic**: Kernel checks `vma->vm_ops`. It is NULL.
    *   *Check: New Check? Uses 04.*
07. **Structure Creation**: Kernel calls `anon_vma_prepare` to allocate `struct anon_vma`.
    *   *Check: New Struct? Derived from logic: need struct for anon.*
08. **Pointer Value**: Allocator returns `struct anon_vma *av = 0xffff888102a9cd20`.
    *   *Check: New Data? Real data from live capture.*
09. **Alignment Check**: `0xffff888102a9cd20 % 8 = 0`. Last 3 bits are `000`.
    *   *Check: New Math? Integer Modulo. 32 mod 8 = 0.*
10. **Flag Definition**: `PAGE_MAPPING_ANON` is defined as `0x1` in `include/linux/page-flags.h`.
    *   *Check: New Value? Axiom Source Provided (Header).*
11. **Encoding Operation**: `stored_value = (unsigned long)av | PAGE_MAPPING_ANON`.
    *   *Check: New Calc? OR operation. 0 | 1 = 1.*
12. **Bitwise Calculation**: `...11100000 | ...0000001` = `...11100001`.
    *   *Check: New Math? Binary OR. Correct.*
13. **Resulting Value**: `0xffff888102a9cd21` (This is the value we read in the driver).
    *   *Check: New Data? Result of 12.*
14. **Decoding - Step 1 (Get Flag)**: `value & 3` -> `...01 & ...11` -> `1`.
    *   *Check: New Math? AND operation. Correct.*
15. **Decoding - Step 2 (Get Ptr)**: `value & ~3` -> `...01 & ...00` -> `...00`.
    *   *Check: New Math? AND operation with mask. Correct.*
16. **Conclusion**: The odd value `...21` inherently proves the page is Anonymous because the only way bit 0 becomes 1 is via this explicit OR operation in the kernel source.
    *   *Check: Final Verdict? Derived from 1-15.*

### STEP 3: FINAL VERDICT
*   Axioms Derived: **YES**
*   Calculations Verified: **YES**
*   Hardware Constraints Met: **YES**
*   **Result:** The "Magic" of the `mapping` field is simply **Integer Math** taking advantage of **Hardware Alignment** rules.

### NEW THINGS INTRODUCED WITHOUT DERIVATION?
*   **None.** Every step traces back to:
    1.  Size of Pointer (8 bytes).
    2.  Alignment Rule (Mod 8 == 0).
    3.  Bitwise AND/OR logic.
    4.  Kernel Source Definitions (MAP_ANON=0x20, PAGE_MAPPING_ANON=0x1).

---

## AXIOMATIC DERIVATION 6: THE GHOST IN THE MACHINE (KERNEL THREADS)

**AXIOM 1:** A CPU runs Instructions.
**AXIOM 2:** A "Process" is a container for Instructions + Memory Maps (`struct mm_struct`).
**AXIOM 3:** The Kernel must run its own instructions (to clean memory, manage disks, merge pages).
**AXIOM 4:** `struct task_struct` is the kernel definition of ANY schedulable entity (Process or Thread).

### DERIVATION: KERNEL THREAD vs USER PROCESS
1.  **Field:** `task->mm`.
    *   Definition: Points to the Userspace Memory Map.
    *   User Process: `task->mm != NULL` (Has valid userspace addresses 0x0...0x7fff...).
2.  **Kernel Thread Definition:** A `task_struct` where `task->mm == NULL`.
    *   **Math:** It has NO userspace mapping.
    *   **Implication:** It cannot run user code. It cannot see user variables variables.
    *   **Permission:** It runs in Ring 0 (Kernel Mode) continuously.
3.  **Access:** How does `ksmd` access User Page X?
    *   It uses the **Direct Map** (Kernel Virtual Address, e.g., `0xffff...`).
    *   User VA (`0x7f...`) -> HW Page Table -> Phys Frame `P`.
    *   Kernel VA (`0xff...`) -> Offset Math -> Phys Frame `P`.
    *   **Result:** `ksmd` accesses the *Physical Data* via its own Window, ignoring the User's Window.

### DERIVATION: THE KSM DAEMON
1.  **Identity:** `ksmd` is a Kernel Thread created at boot (`mm/ksm.c`).
2.  **Job:** It loops forever (while `run=1`).
3.  **List:** It maintains a list of "Scan Items" (Physical addresses added via `madvise`).
4.  **Math:**
    *   Read Page A (via Kernel VA) -> Compute Checksum.
    *   Read Page B (via Kernel VA) -> Compute Checksum.
5.  **Action:** If Checksums match -> Use low-level MMU functions to update Page Tables for *both* User Processes to point to the *same* Physical Frame.
6.  **Update:** It updates the `struct page` metadata (the `mapping` field) to reflect this new "Shared" reality (`PAGE_MAPPING_KSM`).

---
## AXIOMATIC DERIVATION 7: THE PAGEMAP PROOF (PYTHON LOGIC)

**AXIOM 1:** Virtual Memory is an Array of Pages indexed by VPN (Virtual Page Number).
**AXIOM 2:** Physical Memory is an Array of Frames indexed by PFN (Physical Frame Number).
**AXIOM 3:** Hardware stores mapping in a **Hierarchical Tree** (PGD -> PUD -> PMD -> PTE).
**AXIOM 4:** Linux exposes this as a **Flattened Linear Array** in `/proc/PID/pagemap`.

### DERIVATION: THE ABSTRACTION (KERNEL MAGIC)
1.  **Userspace View:** The file appears as a simple array `PFN[Index]`.
    *   *Check: New View? Abstraction derived from Axiom 4 (Flattened Linear Array).*
2.  **Kernel Action on `read()`:**
    *   User requests Index `I`.
        *   *Check: New Variable? Definition of Input.*
    *   Kernel calculates `VA = I * 4096`.
        *   *Check: New Math? Inverse of VPN math below. Derived.*
    *   Kernel performs the hardware walk:
        *   `CR3` -> `PGD Entry`.
            *   *Check: New Hardware? Derived from Axiom 3 (Hierarchical Tree).*
        *   `PGD` -> `PUD Entry`.
        *   `PUD` -> `PMD Entry`.
        *   `PMD` -> `PTE Entry` (The Bottom Level).
    *   Kernel extracts **Final PFN** from the `PTE`.
        *   *Check: New Data? Derived from PTE definition.*
    *   Kernel packs this PFN into the 64-bit response.
        *   *Check: New Action? Derived from API definition.*
3.  **Result:** User sees the final destination without needing to parse the tree structure.
    *   *Check: Conclusion? Follows from 1 & 2.*

### DERIVATION: THE INDEX MATH
1.  **Input:** Virtual Address `VA` (e.g., 4096).
    *   *Check: New Input? Standard variable.*
2.  **Definition:** Page Size `SZ` = 4096 bytes.
    *   *Check: New Const? Standard x86 page size.*
3.  **Calculation:** `VPN = VA / SZ`.
    *   *Check: New Calc? Division to find index. Standard Array Math.*
4.  **Definition:** Pagemap Entry Size `ES` = 8 bytes (64 bits).
    *   *Check: New Const? Derived from Axiom 4 / Derivation 8.*
5.  **Calculation:** `File_Offset = VPN * ES`.
    *   *Check: New Calc? Multiplication for byte offset. Standard Array Math.*

### DERIVATION: THE BITWISE DECODING
1.  **Input:** 64-bit Integer `E` read from file.
    *   *Check: New Variable? Output of previous read.*
2.  **Bit 63:** "Page Present".
    *   Math: `E & (1 << 63)`.
    *   *Check: New Math? Bitshift. Axiom source: Derivation 8.*
3.  **Bits 0-54:** "Physical Frame Number".
    *   Mask: `0x007FFFFFFFFFFFFF` (55 bits).
    *   *Check: New Value? Derived in Derivation 8.*
    *   Math: `PFN = E & Mask`.
    *   *Check: New Math? Bitwise AND.*

### DERIVATION: THE KSM PROOF
1.  **Input:** `VA1` and `VA2` (Different Virtual Addresses).
    *   *Check: New Input? From User Experiment.*
2.  **Process:** Calculate `PFN1` and `PFN2` using steps above.
    *   *Check: New Process? Reuse of INDEX MATH & DECODING sections.*
3.  **Comparison:**
    *   If `PFN1 != PFN2`: They point to different RAM chunks.
        *   *Check: New Logic? Logic of Inequality.*
    *   If `PFN1 == PFN2`: They point to the **SAME** RAM chunk.
        *   *Check: New Logic? Logic of Equality.*
4.  **Conclusion:** If user did not use `mmap(SHARED)`, the only way PFNs match is if the Kernel **Deduplicated** them (KSM).
    *   *Check: Final Verdict? Derived logic.*

---
## AXIOMATIC DERIVATION 8: WHY 55 BITS? (API MATH)

**AXIOM 1:** The Pagemap Entry is strictly **64 bits** (8 bytes).
    *   *Check: New Axiom? API Standard.*
**AXIOM 2:** The Kernel needs to store Metadata Flags AND the Address in these 64 bits.
    *   *Check: New Axiom? Design Requirement.*
**AXIOM 3:** The Flags are stored in the **High Bits** (Top Down).
    *   *Check: New Axiom? API Standard.*

**CALCULATION OF RESERVED BITS:**
1.  **Bit 63:** Page Present (`PM_PRESENT`).
    *   *Check: New Flag? API Standard.*
2.  **Bit 62:** Page Swapped (`PM_SWAP`).
3.  **Bit 61:** Page File/Shared (`PM_FILE`).
4.  **Bit 60:** PTE Soft Dirty (`PM_SOFT_DIRTY` - since 3.11).
5.  **Bit 59:** Page Executable (Reserved).
6.  **Bit 58:** Page Non-Uniform (Reserved).
7.  **Bit 57:** Userfaultfd WP (`PM_UFFD_WP`).
8.  **Bit 56:** Exclusive Mapping (`PM_MMAP_EXCLUSIVE`).
9.  **Bit 55:** Soft Dirty (`PM_SOFT_DIRTY`).

**SUBTRACTION:**
*   Total Bits: 64.
    *   *Check: New Value? Axiom 1.*
*   Reserved Flags: 9 bits (Bits 55 through 63).
    *   *Check: New Calc? Counted listed flags.*
*   Remaining Bits: `64 - 9 = 55 bits`.
    *   *Check: New Calc? Subtraction.*
*   **Result:** Bits **0 through 54** are available for the PFN.
    *   *Check: Conclusion? Follows from Remaining Bits.*

**VERIFICATION:**
*   Hardware Limit (Your Machine): 44 bits.
    *   *Check: New Data? Fetched from /proc/cpuinfo.*
*   Hardware Limit (Modern x86): 52 bits.
    *   *Check: New Const? Hardware Standard.*
*   API Capacity: 55 bits.
    *   *Check: New Const? Derived previously.*
*   **Conclusion:** 55 > 52 > 44. The mask `0x7FFFFFFFFFFFFF` covers ALL possible physical Memory Addresses on any current architecture.
    *   *Check: Final Verdict? Mathematical Inequality Proof.*

---
## PHASE 6: EXECUTION TRACE - KSM (STATE 11)

**TRACE C: KSM PAGE**

01. **Input A:** User allocs Page 1 `VA_1` with content `0xCC` (All bytes).
02. **Input B:** User allocs Page 2 `VA_2` with content `0xCC` (All bytes).
03. **Action:** `madvise(VA_1, MADV_MERGEABLE)` ∧ `madvise(VA_2, MADV_MERGEABLE)`.
04. **Wait:** `sleep(5)` → `ksmd` thread wakes up.
05. **Scan:** `ksmd` hashes `VA_1` → `Hash_X`.
06. **Scan:** `ksmd` hashes `VA_2` → `Hash_X`.
07. **Match:** `Hash_X` == `Hash_X` → **Deduplication Triggered**.
08. **Merge:** Kernel releases Page 2 (Physical) → Maps `VA_2` to Page 1 (Physical).
09. **Status:** Page 1 refcount = 2.
10. **Flagging:** Kernel upgrades `page->mapping`.
    *   **Old:** `struct anon_vma *` | `0x1`.
    *   **New:** `struct ksm_stable_node *`.
    *   **Bits:** `PAGE_MAPPING_KSM` (0x3).
11. **Inspection Input:** `VA_1` = `0x7744f147f000`.
12. **Read Raw:** `0xffff8cf5c4652483` (from dmesg).
13. **Calc Flags:** `0x...483 & 3` → `...0011 & 0011` → `3`.
14. **Check:** `3 == PAGE_MAPPING_KSM` (0x3) ✓.
15. **Calc Ptr:** `0x...483 & ~3` → `...0011 & ...1100` → `...0000`.
16. **Result:** `0xffff8cf5c4652480`.
17. **Align:** `0x...480 % 8 == 0` ✓.
18. **Conclusion:** State 11 Confirmed. Page is KSM backed.

**FINAL: ALL STATES VERIFIED**
*   **State 00:** File (Page Cache).
*   **State 01:** Anon (Heap/Stack).
*   **State 11:** KSM (Deduplicated).


---
## ERROR REPORT (POST-MORTEM)

**ERROR 1: CONFUSION OF LAYERS (HW vs SW)**
*   **Thought:** "My machine has 52 bits max... why 55?"
*   **Root Cause:** Conflating CPU Hardware Limits (CR3/Wiring) with Operating System API Design (Pagemap Interface).
*   **Sloppy:** Assuming software containers must shrink to fit current hardware.
*   **Orthogonal View:** Containers (API) must differ from Content (HW). A bucket (UINT64) is 64 bits regardless of water level (44 bits).
*   **Prevention:** Axiom: "API != Implementation".

**ERROR 2: INVISIBLE MECHANISMS (KSM)**
*   **Thought:** "How does kernel know we never invoked that thing from the driver?"
*   **Root Cause:** Ignoring the existence of background active agents (Kernel Threads aka `ksmd`).
*   **Sloppy:** Viewing the OS as a passive library that only reacts to the Driver's sequential calls.
*   **Orthogonal View:** The OS is a parallel machine. `ksmd` acts while User sleeps.
*   **Prevention:** Axiom: "The Kernel is Multithreaded".

**ERROR 3: EXECUTION BLINDNESS (IO BUFFERS)**
*   **Thought:** "Logs are empty. Command failed."
*   **Root Cause:** Buffered IO in shell pipes. `grep` buffering.
*   **Sloppy:** Not flushing streams (`stdbuf -o0` or `fflush`).
*   **Prevention:** Axiom: "Pipes Buffer. Explicitly Flush."

**ERROR 4: SOURCE FALLACY**
*   **Thought:** "Fetch all kernel sources from my machine."
*   **Root Cause:** assuming `linux-headers` package contains `.c` implementation files.
*   **Sloppy:** Not verifying file existence (`ls`) before asserting availability.
*   **Prevention:** Axiom: "Headers are Interface. Source is Implementation."

**ERROR 5: BINARY ENDING CONFUSION**
*   **Thought:** "0x...d21 & ~3 = 0x...d20. now it ended in 1 not 01"
*   **Root Cause:** Imprecise language regarding Binary vs Hex vs Decimal.
*   **Sloppy:** "Ends in 1" is ambiguous (Bit? Byte? Hex char?).
*   **Prevention:** Axiom: "Always specify Base. Binary 01 != Hex 01."
