:ERROR REPORT #001
00. MISTAKE: Conflating hardware register (CR3) with kernel's software interface (/proc/self/pagemap).
01. SYMPTOM: Confusion over "54 bits" vs "40 bits".
02. ROOT CAUSE: User assumed pagemap format mirrors CR3 physical address format.
03. FACT: They are SEPARATE. Diagram below.

:DIAGRAM 1: WHY THE CONFUSION?
+--------------------------------------------------------------------------------------------------+
| CONCEPT A: CR3 REGISTER (HARDWARE)                                                               |
| What: Hardware register holding physical address of the Page Global Directory (PGD).             |
| Where: CPU internal register. Read via `movq %cr3, %rax`.                                        |
| Width: Depends on CPU's "address sizes". On THIS machine: 44 bits physical.                      |
| Source: `cat /proc/cpuinfo | grep "address sizes"` → `44 bits physical, 48 bits virtual`.       |
+--------------------------------------------------------------------------------------------------+
                  |
                  |  ≠ NOT EQUAL
                  |
+--------------------------------------------------------------------------------------------------+
| CONCEPT B: /proc/self/pagemap (KERNEL SOFTWARE INTERFACE)                                        |
| What: A file interface to READ page table entries from userspace. Defined by KERNEL.             |
| Where: Kernel source `fs/proc/task_mmu.c`.                                                       |
| Width: Kernel defines `PM_PFRAME_BITS = 55`. Mask = `GENMASK_ULL(54, 0)` = bits 0-54.            |
| Source: `/home/r/Desktop/learn_kernel/source/linux/fs/proc/task_mmu.c` Line 1847.                |
+--------------------------------------------------------------------------------------------------+

:DIAGRAM 2: THE 64-BIT PAGEMAP ENTRY LAYOUT (KERNEL-DEFINED)
+----+----+----+----+----+----+----+----+----+----+ ... +----+----+----+----+
| 63 | 62 | 61 | 60 | 59 | 58 | 57 | 56 | 55 | 54 | ... |  1 |  0 |        |
+----+----+----+----+----+----+----+----+----+----+ ... +----+----+----+----+
|PRES|SWAP|FILE| R  | R  |GUAR|WFFD| EX |SOFT|                PFN          |
+----+----+----+----+----+----+----+----+----+-----------------------------+
Bit 63: PM_PRESENT (Page in RAM).
Bit 62: PM_SWAP (Page in swap).
Bit 61: PM_FILE (Page is file-backed).
Bit 58: PM_GUARD_REGION.
Bit 57: PM_UFFD_WP (Userfaultfd write-protected).
Bit 56: PM_MMAP_EXCLUSIVE.
Bit 55: PM_SOFT_DIRTY.
Bits 0-54: PFN (Page Frame Number).

:DERIVATION: WHY 55 BITS FOR PFN?
04. AXIOM: Page Size = 4096 = 2^12.
05. FACT: Max User Virtual Address (48-bit) / Page Size = 2^48 / 2^12 = 2^36 max VPNs.
06. FACT: Kernel designers chose 55 bits to allow for FUTURE expansion of physical memory.
07. FACT: 55 bits for PFN → Max Addressable = 2^55 * 4096 = 2^67 bytes.
08. FACT: This machine has 44 physical address bits → Max RAM = 2^44 = 16 TB.
09. FACT: PFN on this machine fits in 44-12 = 32 bits. Bits 32-54 of PFN are zeros.
10. ∴ The 55-bit field is a SOFTWARE generalization. It is NOT tied to THIS CPU's CR3.

:NUMERICAL EXAMPLE (THIS MACHINE)
11. CPU Physical Bits: 44.
12. Max Physical Address: 2^44 - 1 = 0xFFFFFFFFFFF.
13. Max PFN = Max Phys >> 12 = 0xFFFFFFFFFFF >> 12 = 0xFFFFFFFF (32 bits).
14. In pagemap entry: PFN fits in bits 0-31. Bits 32-54 = 0.
15. Entry example: 0x8180000000123456.
16. Bit 63 = 1 → Present.
17. Bits 0-54 = 0x0000000000123456 → PFN = 0x123456.

:YOUR MISTAKE LOG
18. CARELESS READING: Assumed hardware defines pagemap format. ✗.
19. SLOPPY INFERENCE: Confused CR3 (phys addr of page table) with pagemap entry (kernel abstraction). ✗.
20. MISSED DOCUMENTATION: Did not read `task_mmu.c` which defines the format. ✗.
21. MUMBO JUMBO TYPING: Writing code without understanding the 55 vs 44 distinction. ✗.
22. FIX: Always trace constant definitions back to kernel source.

:CONVERSATION SNAPSHOT (2026-01-01 16:34)
:TOPIC: PFN vs PTE vs Pagemap bit widths

:LIVE DATA FROM THIS MACHINE
23. `cat /proc/cpuinfo | grep "address sizes"` → 44 bits physical, 48 bits virtual.

================================================================================
:AXIOMATIC DERIVATION - REVERSE MAPPING (struct page → VMA) (2026-01-01 19:48)
================================================================================

:GOAL: Given a struct page, find the VMA that allocated it.
:CONSTRAINT: Start from primitives. Each line uses ONLY prior lines.

:AXIOM A0: Byte = 8 bits. Address = 64-bit integer on x86_64.
:AXIOM A1: Pointer = 64-bit value holding a memory address.
:AXIOM A2: Struct = contiguous block of bytes at some address.
:AXIOM A3: Struct field = bytes at (struct address + offset).

24. struct page = kernel struct describing ONE 4096-byte physical page.
25. Source: mm_types.h:78. struct page { memdesc_flags_t flags; union { ... } }
26. struct page has field `mapping` at some offset.
27. `mapping` is type: struct address_space* (pointer, 8 bytes).

:DERIVATION OF MAPPING FIELD TRICK
28. When kernel allocates an anonymous page (heap, stack, malloc):
29. Kernel stores pointer to `struct anon_vma` in `page->mapping`.
30. BUT kernel sets Bit 0 = 1 to signal "this is Anonymous, not File".
31. Proof: page-flags.h:717. #define FOLIO_MAPPING_ANON 0x1

:DERIVATION STEP BY STEP
32. Let page = pointer to struct page = 0xffffea0001234000.
33. Let page->mapping = 0xffff888012345681 (ODD, Bit 0 = 1).
34. Check: 0xffff888012345681 & 1 = 1. ∴ Anonymous page.
35. Remove Bit 0: 0xffff888012345681 & ~1 = 0xffff888012345680.
36. Cast: (struct anon_vma*)0xffff888012345680.
37. NOW we have pointer to struct anon_vma.

:DEFINITION OF struct anon_vma (SOURCE: rmap.h:32-68)
38. struct anon_vma {
39.   struct anon_vma *root;         // Offset 0, 8 bytes. Points to tree root.
40.   struct rw_semaphore rwsem;     // Offset 8, ~40 bytes. Lock.
41.   atomic_t refcount;             // Offset ~48, 4 bytes. Reference count.
42.   unsigned long num_children;    // Offset ~52, 8 bytes. Child count.
43.   unsigned long num_active_vmas; // Offset ~60, 8 bytes. VMA count.
44.   struct anon_vma *parent;       // Offset ~68, 8 bytes. Parent pointer.
45.   struct rb_root_cached rb_root; // Offset ~76, 16 bytes. Tree of VMAs.
46. }

:DEFINITION OF rb_root_cached
47. rb_root_cached = struct { struct rb_node *rb_node; struct rb_node *rb_leftmost; }
48. rb_node = pointer to root of Red-Black tree.
49. Red-Black tree = balanced binary search tree. O(log N) lookup.
50. Each node in tree is: struct anon_vma_chain.

:DEFINITION OF struct anon_vma_chain (SOURCE: rmap.h:83-92)
51. struct anon_vma_chain {
52.   struct vm_area_struct *vma;    // Offset 0, 8 bytes. ← THIS IS THE VMA!
53.   struct anon_vma *anon_vma;     // Offset 8, 8 bytes. Back-pointer.
54.   struct list_head same_vma;     // Offset 16, 16 bytes. List node.
55.   struct rb_node rb;             // Offset 32, 24 bytes. Tree node.
56.   unsigned long rb_subtree_last; // Offset 56, 8 bytes. Interval tree.
57. }

:FULL TRACE (PAGE → VMA)
58. START: struct page* page = 0xffffea0001234000.
59. STEP 1: Read page->mapping = 0xffff888012345681.
60. STEP 2: Check (mapping & 1) == 1. YES → Anonymous page.
61. STEP 3: anon_vma ptr = mapping & ~1 = 0xffff888012345680.
62. STEP 4: struct anon_vma* av = (struct anon_vma*)0xffff888012345680.
63. STEP 5: Read av->rb_root.rb_node (offset ~76).
64. STEP 6: Walk Red-Black tree. Each node is anon_vma_chain.
65. STEP 7: Read avc->vma (offset 0 of anon_vma_chain).
66. STEP 8: avc->vma = 0xffff8880abcdef00. ← THIS IS THE VMA.

:SUMMARY CHAIN
page → mapping (& ~1) → anon_vma → rb_root → anon_vma_chain → vma

================================================================================
:ERROR REPORT #005 - SLOPPY BRAIN ANALYSIS (2026-01-01 20:51)
================================================================================

:MISTAKE 01: "how does one go from pte fro the vma"
  Line: User Prompt Step 461.
  Wrong: Assumed PTE contains metadata or pointers to VMA.
  Right: PTE is a u64 integer. Contains ONLY PFN + Flags. No back-pointers.
  Sloppy: Did not look at PTE definition (arch/x86/include/asm/pgtable_64.h).
  Prevent: "grep" struct/typedef definitions before assuming fields exist.

:MISTAKE 02: "how does one know for a given struct page what was the vma"
  Line: User Prompt Step 461.
  Wrong: Confused "page made by VMA" with "page mapped by VMA".
  Right: Anon pages created by Fault → anon_vma. File pages created by FS → address_space.
  Sloppy: Ignoring `page->mapping` semantics defined in mm_types.h.
  Prevent: Read struct definitions. `struct page` has `mapping`. Tracing `mapping` finds VMA.

:MISTAKE 03: "you are introdcuing new ideas without deriving them"
  Line: User Prompt Step 464.
  Wrong: Rejection of necessary vocabulary (`anon_vma`).
  Right: Impossible to explain "Page → VMA" without `anon_vma`. It IS the link.
  Sloppy: Thinking explanation is "magic" rather than "data structure traversal".
  Prevent: Accept that new Structs = New Axioms.

:MISTAKE 04: "check" (Disk Full)
  Line: User Prompt Step 513.
  Wrong: Ignoring write errors.
  Right: `cat: write error: No space left on device`.
  Sloppy: Blindly moving forward when filesystem screams STOP.
  Prevent: Read error logs. 

:ORTHOGONAL THOUGHT PROCESS
  State 0: "I want to find VMA from Page."
  Fact 0: Page does not store VMA pointer directly (too expensive, 8 bytes per page).
  Fact 1: Page stores `mapping` (shared).
  Fact 2: `mapping` must point to something that points to VMA.
  Deduction: `mapping` → [Intermediate Struct] → VMA.
  Name it: Intermediate Struct = `anon_vma` (Anon) or `address_space` (File).
  Conclusion: No magic. Just indirection to save memory.


================================================================================
================================================================================
:GRAND UNIFICATION TRACE (STRICT AXIOMATIC REVISION) (2026-01-02 11:00)
================================================================================

:GOAL: Trace integers from User VA -> RAM -> VMA with REAL DATA.
:CONSTRAINT: Start from primitives (Integers, Pointers). No new words without derivation.

:SECTION 1: THE FORWARD TRACE (VA -> PHYS)
:DATA SOURCE: Live execution of `mm_exercise_user` (PID 20814).
:VALUE VA: 0x7c1f1de05000 (Measured from /proc/20814/maps).

:STEP 1.1: SPLIT THE INTEGER (VA)
1. User VA is an integer: 0x7c1f1de05000.
2. Machine is x86_64. It uses 4 levels of translation tables.
3. Level 1 (PGD Index) = bits 39-47.
   Calculation: (0x7c1f1de05000 >> 39) & 0x1FF = 0xF8 (Decimal 248).
4. Level 2 (PUD Index) = bits 30-38.
   Calculation: (0x7c1f1de05000 >> 30) & 0x1FF = 0x7C (Decimal 124).
5. Level 3 (PMD Index) = bits 21-29.
   Calculation: (0x7c1f1de05000 >> 21) & 0x1FF = 0xEF (Decimal 239).
6. Level 4 (PTE Index) = bits 12-20.
   Calculation: (0x7c1f1de05000 >> 12) & 0x1FF = 0x05 (Decimal 5).

:STEP 1.2: READ THE PHYSICAL RESULT (PFN)
7. We traced this lookup via `pagemap` file (simulating CPU hardware walk).
8. Result (Raw Entry): 0x818000000003ca9c.
9. Bit 63 (Present) = 1.
10. Bits 0-54 (PFN) = 0x3ca9c.
11. Physical Addr = PFN * 4096 = 0x3ca9c * 0x1000 = 0x3ca9c000.
12. Value at 0x3ca9c000: 0xAA (The byte we wrote).

:SECTION 2: THE REVERSE TRACE (PHYS -> VMA)
:PROBLEM: We have PFN 0x3ca9c. We need to find the OWNER (VMA).

:STEP 2.1: FIND THE METADATA (struct page)
13. Kernel stores an array of structs called `mem_map` or `vmemmap`.
14. `struct page` address = Base + (PFN * 64).
15. This struct contains the field `mapping`.

:STEP 2.2: DECODE THE MAPPING
16. Read `page->mapping`. Value is a pointer (integer).
17. Check Bit 0.
    IF (mapping & 1):
      It is an ANONYMOUS PAGE.
      Owner Object = (struct anon_vma*)(mapping - 1).
    ELSE:
      It is a FILE PAGE.
      Owner Object = (struct address_space*)mapping.

:SECTION 3: DERIVATION OF TREES (WHY DO WE NEED THEM?)
:AXIOM T0: A Process has N memory areas (VMAs).
:AXIOM T1: CPU must find the correct VMA for every access (to check permissions).

:SCENARIO A: LINEAR LIST (The Naive Way)
18. Process has 1000 VMAs.
19. Storage: Linked List. VMA1 -> VMA2 -> VMA3 ...
20. Operation: Find VMA for Address X.
21. Cost: Check VMA1 (No). Check VMA2 (No) ... Check VMA500 (Yes).
22. Average Steps: N / 2 = 500 steps.
23. Total Accesses per second: 1,000,000.
24. Total Cost: 500 * 1,000,000 = 500,000,000 checks.
25. CONCLUSION: Too slow.

:SCENARIO B: BINARY SEARCH (The Better Way)
26. Storage: Sorted Array or Tree.
27. Cost: log2(N).
28. For N=1000: log2(1000) ≈ 10 steps.
29. Total Cost: 10 * 1,000,000 = 10,000,000 checks.
30. CONCLUSION: 50x faster than List.
31. IMPLEMENTATION: Red-Black Tree (Balanced Binary Tree).

:SCENARIO C: THE GAP PROBLEM (Why Maple Tree?)
32. Operation: `mmap(size=4096)`. Needs to find a FREE HOLE.
33. RB Tree (Standard): Stores allocated items.
    [0-4k] [8k-12k] [16k-20k]
34. Algorithm:
    - Visit Node [8k-12k]. Is there space left? No.
    - Visit Node [0-4k]. Is there space right? Yes (4k-8k).
35. Problem: In RB Tree, "gap info" is not stored. Must traverse to find gaps.
36. Worst case: O(N) linear scan of sorted nodes to find a gap.
37. SOLUTION: Structure that stores GAPS in the nodes.
38. NEW STRUCTURE: MAPLE TREE.
    - Node contains: [Range 0-4k, MaxGap=0] [Range 8k-12k, MaxGap=4k]
    - Search: "Find Node with MaxGap >= Request".
    - tracing: O(log N).
39. FACT: Linux 6.1 moved from RB Tree to Maple Tree for `mm_struct` to solve this GAP problem.

:SUMMARY OF STRUCTURES
40. Forward (mm_struct): Maple Tree (Optimized for Gap Search).
41. Reverse (anon_vma): RB Tree / Interval Tree (Optimized for Overlap Search).
================================================================================

:GOAL: Trace integers from User VA -> RAM -> VMA with REAL DATA.
:DATA SOURCE: Live execution of `mm_exercise_user` (PID 20814).

:TRACE 1: FORWARD PATH (THE CPU WALK)
  User VA: 0x7c1f1de05000 (from maps).
  Logic: CPU splits 48-bit VA into 4 indices + offset.
  
  Binary Split (48 bits):
  011111000 001111100 011101111 000000101 000000000000
  |_______| |_______| |_______| |_______| |__________|
     PGD       PUD       PMD       PTE      OFFSET

  Step 1. CR3 (Phys) + PGD_Index(248) * 8 -> PGD Entry.
          PGD Entry contains Phys Addr of PUD Table.
  Step 2. PUD_Phys + PUD_Index(124) * 8 -> PUD Entry.
          PUD Entry contains Phys Addr of PMD Table.
  Step 3. PMD_Phys + PMD_Index(239) * 8 -> PMD Entry.
          PMD Entry contains Phys Addr of Page Table.
  Step 4. PT_Phys  + PTE_Index(5)   * 8 -> PTE Entry.
          PTE Entry contains PFN + Flags.

  :REAL DATA (From /proc/20814/pagemap):
  Offset calculated: 0x7c1f1de05000 >> 12 = 2082414085.
  Raw Entry read: 0x818000000003ca9c.
  
  DECODING 0x818000000003ca9c:
  Bit 63 (Present): 1 (Page is in RAM).
  Bits 0-54 (PFN): 0x3ca9c (Decimal 248476).
  
  :PHYSICAL RAM ADDRESS
  Phys = PFN * 4096 = 0x3ca9c * 0x1000 = 0x3ca9c000.
  Value at 0x3ca9c000: 0xAA (We wrote this byte).

:TRACE 2: REVERSE PATH (THE KERNEL LOOKUP)
  Goal: Packet of data arrives at Phys 0x3ca9c000. Which Process owns it?
  
  Step 1. Phys Base -> struct page.
          page = vmemmap + PFN(0x3ca9c) * sizeof(struct page).
          
  Step 2. struct page -> mapping.
          Read `page->mapping`.
          IF (mapping & 1): It is ANONYMOUS.
          Target = (struct anon_vma*)(mapping & ~1).
          
  Step 3. anon_vma -> VMA (Why Trees?).
          We have `anon_vma`. It represents "The Object".
          But which *Range*? Note: `fork()` makes multiple VMAs share same `anon_vma`.
          We need to find the VMA that covers index `pgoff`.
          
          Structure: `anon_vma->rb_root` (Red-Black Tree).
          Search: Walk RB Tree. Compare `vma->vm_pgoff`.
          Found Node: `struct anon_vma_chain`.
          Result: `anon_vma_chain->vma`.
          
:DERIVATION: WHY TREES? (The "Why" Question)

  :1. WHY MAPLE TREE? (Forward: mm_struct -> vma)
      Old way (RB Tree): Good for finding exact match. Bad for "Find Free Gap".
      Problem: `mmap` needs to find "Unused space between 0x1000 and 0x8000".
      RB Tree: Must walk entire tree to find gaps. Slow O(N).
      Maple Tree: Stores RANGES and GAPS directly in nodes.
      Result: `mmap` is O(log N). Fast gap search.
      Current Kernel (6.14): `mm_struct` uses `mm_mt` (Maple Tree).

  :2. WHY INTERVAL/RB TREE? (Reverse: page -> vma)
      Problem: Physical Page P belongs to file "libc.so".
      "libc.so" is mapped by 100 processes (firefox, bash, python).
      If we swap out Page P, we must modify PTEs in ALL 100 processes.
      Naive List: Linked List of VMAs. O(N).
      1000 processes? swapping takes 1000 steps. Bad.
      Solution: Interval Tree (struct address_space) or RB Tree (struct anon_vma).
      Process:
        1. Look up File Object (`address_space`).
        2. Search Interval Tree for range [Offset, Offset+4096].
        3. Returns strictly the overlapping VMAs (e.g., 100 matches).
      Result: Efficient lookup of sharing processes.

  :SUMMARY OF FLOW
  Forward: VA -> [Maple Tree] -> VMA.
  Forward: VA -> [Page Table Walk] -> Phys.
  Reverse: Phys -> [struct page] -> [RB/Interval Tree] -> VMA.


================================================================================
:AXIOMATIC VERIFICATION SESSION (2026-01-02 11:05)
================================================================================

**Status**: Verified on Linux 6.14.0
**Axioms Verified**: [PTE Mask](./pte_mask_derivation.md), [Bit Width](./bit_width_derivation.md), [Session Log](./session_worksheet.md)

## The Hypothesis
When you call `malloc(4096)`, you do not get 4096 bytes of RAM. You get a promise.
This investigation proves that promise is broken (and fulfilled) via the Page Fault mechanism.
See also: [Fault at Offset Trace](./fault_at_offset_trace.md).

## 1. The Virtual Lie
Userspace programs see a contiguous, linear address space.
Process `mm_exercise_user` (PID 1234) sees `0x7b9312dd6000`.
  Syscall: `mmap(NULL, ...)` -> Kernel returns available slot.
  Value: `0x73de0912a000`.

:GOAL: Prove that Manual Calculation of Indices matches Kernel Hardware Reality.
:METHOD:
  1. Generate VA in userspace (Axiom 0).
  2. Calculate PGD Index manually (Axiom 1).
  3. Modify Kernel to dump real PGD Index (The Proof).
  4. Compare.

:STEP 1: AXIOM 0 - THE INTEGER
  Source: `mm_exercise_user` (PID 26850).
  Syscall: `mmap(NULL, ...)` -> Kernel returns available slot.
  Value: `0x73de0912a000`.

:STEP 2: AXIOM 1 - MANUAL CALCULATION
  Formula: PGD Index uses bits 39-47.
  Operation: `(VA >> 39) & 0x1FF`.
  Calculation:
    0x73de0912a000 = 0111 0011 1101 ...
    Shift Right 39 = 00...00011100111
    Binary 011100111 = 231 (Decimal) = 0xE7 (Hex).
  PREDICTION: The kernel MUST check slot #231 in the top-level table.

:STEP 3: THE KERNEL PROOF
  Tool: `mm_exercise_hw.ko` modified to implement `MM_AXIOM_IOC_WALK_VA`.
  Mechanism: `pgd_offset(mm, va)` macro inside kernel.
  
  :DMESG OUTPUT (The Truth):
  [ 7066.990885] ================ [ AXIOM WALK: 0x73de0912a000 ] ================
  [ 7066.990893] L1 PGD: Address = ... | Index = 231 (0xe7) | Val = 0x10f44e067
  [ 7066.990897] L2 PUD: Address = ... | Index = 376 (0x178) | Val = 0x1516de067
  [ 7066.990900] L3 PMD: Address = ... | Index = 72 (0x48)   | Val = 0x13cea0067
  [ 7066.990903] L4 PTE: Address = ... | Index = 298 (0x12a) | Val = 0x800000034a37d867

:CONCLUSION
  Manual Calculation: 0xE7.
  Kernel Reality:     0xE7.
  Status: MATCH ✓.
  
  We have proven, starting from an empty integer, via Axioms of Bit Shifting,
  exactly where the hardware goes to find memory.
  
