# MAPLE TREE WALK WORKSHEET

## MAPLE TREE STRUCTURE DIAGRAM (YOUR CASE: 1 VMA)

```
+--mm_struct--+      +--maple_tree (mm_mt)--+      +--maple_range_64 NODE--+      +--vm_area_struct--+
|             |      |                      |      | parent = NULL        |      | vm_start=0x78d7ce727000 |
| mm_mt ------+----->| ma_flags = 0x0       |      | pivot[0]=0x78d7ce727fff |->| vm_end  =0x78d7ce728000 |
|             |      | ma_lock  = 0         |      | pivot[1..14] = 0     |      | vm_flags=0x73           |
|             |      | ma_root  = 0x...010 -+----->| slot[0] = 0x...0000 -+----->|                         |
+-------------+      +----------------------+      | slot[1..15] = NULL   |      +-------------------------+
                                                   +----------------------+
                                                   (256 bytes, 4 cache lines)
```

## MAPLE TREE WITH MANY VMAs (17+ VMAs, 2 levels)

```
                         mm->mm_mt.ma_root = 0xffff888200000010
                                    |
                                    v
            +--------INTERNAL NODE (maple_range_64)--------+
            | pivot[0]=0x7fff | pivot[1]=0xffff | ...     |
            | slot[0]         | slot[1]         | slot[2] |
            +--------+----------------+-----------+--------+
                     |                |           |
                     v                v           v
           +-LEAF NODE-+    +-LEAF NODE-+    +-LEAF NODE-+
           |pivot[0]=0x1fff| |pivot[0]=0x9fff| |pivot[0]=0x11fff|
           |slot[0]→VMA_A  | |slot[0]→VMA_Q  | |slot[0]→VMA_AG  |
           |slot[1]→VMA_B  | |slot[1]→VMA_R  | |slot[1]→VMA_AH  |
           |...            | |...            | |...             |
           |slot[15]→VMA_P | |slot[15]→VMA_AF| |slot[15]→VMA_AV |
           +---------------+ +---------------+ +----------------+

## INTEGER FLOW: STEP-BY-STEP VMA LOOKUP

```

INPUT: faulting_addr = 0x78d7ce727100 (from CR2 register)

STEP 1: Read ma_root
  Address to read: &mm->mm_mt.ma_root (somewhere in kernel RAM)
  Value read:      0xffff888200000010

STEP 2: Extract node address from ma_root
  Calculation:     0xffff888200000010 & 0xFFFFFFFFFFFFFF00
  Result:          0xffff888200000000 ← this is the node address

STEP 3: Extract type from ma_root
  Calculation:     (0x10 >> 3) & 0xF = (16 >> 3) & 15 = 2 & 15 = 2
  Result:          Type 2 = maple_range_64

STEP 4: Read pivot[0] from node
  Address to read: 0xffff888200000000 + 8 = 0xffff888200000008
  Value read:      0x78d7ce727fff ← this is vm_end - 1

STEP 5: Compare faulting_addr to pivot[0]
  Comparison:      0x78d7ce727100 <= 0x78d7ce727fff ?
  Subtraction:     0x78d7ce727fff - 0x78d7ce727100 = 0xEFF (positive)
  Result:          TRUE → answer is in slot[0]

STEP 6: Read slot[0] from node
  Offset calc:     parent(8) + pivots(15×8=120) + slot_index(0×8) = 128
  Address to read: 0xffff888200000000 + 128 = 0xffff888200000080
  Value read:      0xffff8881abcd0000 ← this is VMA pointer

STEP 7: Verify VMA range
  Read vm_start:   RAM[0xffff8881abcd0000 + 0] = 0x78d7ce727000
  Read vm_end:     RAM[0xffff8881abcd0000 + 8] = 0x78d7ce728000
  Check lower:     0x78d7ce727100 >= 0x78d7ce727000 ? TRUE
  Check upper:     0x78d7ce727100 <  0x78d7ce728000 ? TRUE

OUTPUT: VMA found at 0xffff8881abcd0000 ✓

```

## RAM LAYOUT: ACTUAL BYTES IN MEMORY

```

RAM ADDRESS                BYTES (8 bytes = 1 integer)        WHAT IT IS
─────────────────────────────────────────────────────────────────────────────
mm_struct somewhere in kernel RAM:
0xffff888123456780+168     10 00 00 00 02 88 ff ff            ma_root = 0xffff888200000010
                           │                                  (little-endian)
                           └─ low byte 0x10 = type bits

maple_range_64 node at 0xffff888200000000:
0xffff888200000000         00 00 00 00 00 00 00 00            parent = NULL
0xffff888200000008         ff 7f 72 ce d7 78 00 00            pivot[0] = 0x78d7ce727fff
0xffff888200000010         00 00 00 00 00 00 00 00            pivot[1] = 0 (unused)
...
0xffff888200000080         00 00 cd ab 81 88 ff ff            slot[0] = 0xffff8881abcd0000
0xffff888200000088         00 00 00 00 00 00 00 00            slot[1] = NULL (unused)
...

vm_area_struct at 0xffff8881abcd0000:
0xffff8881abcd0000         00 70 72 ce d7 78 00 00            vm_start = 0x78d7ce727000
0xffff8881abcd0008         00 80 72 ce d7 78 00 00            vm_end = 0x78d7ce728000
0xffff8881abcd0010         ?? ?? ?? ?? ?? ?? ?? ??            vm_mm = pointer to mm_struct
...
0xffff8881abcd0020         73 00 00 00 00 00 00 00            vm_flags = 0x73

```

## DATA PATH THROUGH RAM

```

CPU register CR2 = 0x78d7ce727100 (faulting address)
                   │
                   v
READ RAM[mm+168] ──────────────> 0xffff888200000010 (ma_root)
                                 │
                                 v
MASK & SHIFT ──────────────────> node=0xffff888200000000, type=2
                                 │
                                 v
READ RAM[node+8] ──────────────> 0x78d7ce727fff (pivot[0])
                                 │
                                 v
COMPARE: 0x78d7ce727100 <= 0x78d7ce727fff? ──> YES → use slot[0]
                                 │
                                 v
READ RAM[node+128] ────────────> 0xffff8881abcd0000 (VMA pointer)
                                 │
                                 v
READ RAM[VMA+0] ───────────────> 0x78d7ce727000 (vm_start)
READ RAM[VMA+8] ───────────────> 0x78d7ce728000 (vm_end)
                                 │
                                 v
VERIFY: 0x78d7ce727000 <= 0x78d7ce727100 < 0x78d7ce728000 ──> TRUE ✓

```

1. STEP 1 AXIOMATIC TRACE:
A1. AXIOM: 1 byte = 8 bits.
A2. AXIOM: RAM = array of bytes, index 0 to N.
A3. AXIOM: Pointer = integer that indexes into RAM.
A4. AXIOM: NULL = 0 = "points to nothing".
A5. AXIOM: Process = running program.
A6. AXIOM: mm_struct = data structure describing process memory. (SOURCE: mm_types.h:767)
A7. AXIOM: mm_mt = field inside mm_struct, type struct maple_tree. (SOURCE: mm_types.h:781)
A8. AXIOM: struct maple_tree has field ma_root (pointer). (SOURCE: maple_tree.h:225)
A9. AXIOM: fork() -> mm_init() -> mt_init_flags(&mm->mm_mt) (SOURCE: fork.c:1260)
A10. AXIOM: mt_init_flags sets ma_root = NULL. (SOURCE: maple_tree.h:237)
A11. DERIVATION: At t=0 (before mmap), ma_root = NULL.
A12. DERIVATION: NULL means tree empty.
A13. DERIVATION: Empty tree means 0 VMAs stored.
∴ DRAW: `+--maple_tree--+` `| ma_flags=0x0 |` `| ma_lock=0 |` `| ma_root=NULL |` `+--------------+`

2. STEP 2 AXIOMATIC TRACE:
A1. AXIOM: mmap() syscall takes 6 arguments.
A2. AXIOM: Argument 2 = length in bytes. (SOURCE: man mmap)
A3. AXIOM: Your code: mmap(NULL, 4096, ...). (SOURCE: mm_exercise_user.c:81)
A4. DERIVATION: length = 4096 bytes.
A5. AXIOM: 4096 in hex = 0x1000. (CALCULATION: 4096 = 4*1024 = 4*0x400 = 0x1000)
A6. AXIOM: mmap returns start address of allocated region.
A7. AXIOM: Your program printed: VA: 0x78d7ce727000. (SOURCE: line 88 output)
A8. DERIVATION: vm_start = 0x78d7ce727000.
A9. AXIOM: VMA range is [vm_start, vm_end). End is EXCLUSIVE.
A10. AXIOM: vm_end = vm_start + length. (SOURCE: kernel convention)
A11. CALCULATION: vm_end = 0x78d7ce727000 + 0x1000.
A12. CALCULATION: 0x78d7ce727000 + 0x1000 = 0x78d7ce728000.
A13. VERIFY: 0x78d7ce728000 - 0x78d7ce727000 = 0x1000 = 4096 ✓.
∴ vm_end = 0x78d7ce728000.

3. STEP 3 AXIOMATIC TRACE:
A1. AXIOM: mmap() creates struct vm_area_struct in kernel RAM.
A2. AXIOM: Kernel RAM addresses >= 0xffff800000000000 on x86_64.
A3. AXIOM: struct vm_area_struct defined at mm_types.h:649.
A4. AXIOM: First field is vm_start (8 bytes). (SOURCE: mm_types.h:655)
A5. AXIOM: Second field is vm_end (8 bytes). (SOURCE: mm_types.h:656)
A6. DERIVATION: Offset of vm_start = 0.
A7. DERIVATION: Offset of vm_end = 0 + 8 = 8.
A8. AXIOM: vm_mm field is pointer to mm_struct. (SOURCE: mm_types.h:663)
A9. DERIVATION: Offset of vm_mm = 8 + 8 = 16.
A10. AXIOM: vm_flags field stores permission bits. (SOURCE: mm_types.h:671)
A11. AXIOM: PROT_READ = 0x1. (SOURCE: man mmap)
A12. AXIOM: PROT_WRITE = 0x2. (SOURCE: man mmap)
A13. AXIOM: Your mmap: PROT_READ | PROT_WRITE = 0x1 | 0x2 = 0x3.
A14. AXIOM: Kernel translates PROT_to VM_ flags.
A15. AXIOM: VM_READ = 0x01. (SOURCE: mm.h:270)
A16. AXIOM: VM_WRITE = 0x02. (SOURCE: mm.h:271)
A17. AXIOM: VM_MAYREAD = 0x10. (SOURCE: mm.h:276)
A18. AXIOM: VM_MAYWRITE = 0x20. (SOURCE: mm.h:277)
A19. AXIOM: VM_MAYEXEC = 0x40. (SOURCE: mm.h:278)
A20. CALCULATION: vm_flags = 0x01 + 0x02 + 0x10 + 0x20 + 0x40.
A21. CALCULATION: 0x01 + 0x02 = 0x03.
A22. CALCULATION: 0x03 + 0x10 = 0x13.
A23. CALCULATION: 0x13 + 0x20 = 0x33.
A24. CALCULATION: 0x33 + 0x40 = 0x73.
A25. DERIVATION: vm_flags = 0x73.
A26. From Step 2 A8: vm_start = 0x78d7ce727000.
A27. From Step 2 A12: vm_end = 0x78d7ce728000.
∴ VMA struct: `+--vm_area_struct--+` `| vm_start=0x78d7ce727000 |` `| vm_end=0x78d7ce728000 |` `| vm_flags=0x73 |` `+--------------------+`

4. STEP 4 AXIOMATIC TRACE:
MAPLE TREE FUNDAMENTALS:
B1. PROBLEM: Need to store VMAs and find which VMA contains a given address FAST.
B2. LINKED LIST O(N) PROOF: 5 VMAs in list. Query: find VMA for addr. Check VMA_0? NO. Check VMA_1? NO... Check VMA_4? YES. 5 checks. WORST CASE: N checks for N elements. ∴ O(N).
B2a. NUMERICAL EXAMPLE: N=1000 VMAs, worst case = 1000 comparisons.
B3. BINARY TREE O(log N) PROOF: Each node has 2 children. Balanced tree depth d holds 2^d - 1 elements. INVERT: N elements → depth = log₂(N+1).
B3a. NUMERICAL EXAMPLE: N=1000. log₂(1001) ≈ log₂(1024) = 10. ∴ 10 comparisons worst case.
B3b. MAPLE TREE (16-way branching): 1 node = 16 slots. 2 levels = 16×16 = 256. 3 levels = 16³ = 4096.
B3c. NUMERICAL EXAMPLE: N=1000. 16^2=256 < 1000 < 4096=16^3. ∴ 3 levels. ∴ 3 RAM reads max.
B3d. COMPARE: Binary tree = 10 reads. Maple tree = 3 reads. Maple is 3× faster.
B4. HISTORY (KERNEL < 6.1): Used rbtree (red-black tree). SOURCE: mm_types.h:553 still has `struct rb_node vm_rb` for file-backed mappings.
B4a. RBTREE AXIOM 1: Binary Search Tree. Each node: KEY, LEFT child, RIGHT child.
B4b. RBTREE AXIOM 2: BST Property: LEFT subtree keys < node KEY < RIGHT subtree keys.
B4c. RBTREE AXIOM 3: For VMAs, KEY = vm_start. Nodes also store vm_end for range check.
B4d. RBTREE STRUCTURE: 5 VMAs example: VMA_C(0x5000) at root, VMA_B(0x3000)/VMA_D(0x7000) as children, VMA_A(0x1000)/VMA_E(0x9000) as leaves.
B4e. RBTREE LOOKUP for addr=0x7500: Step1: root=VMA_C, 0x7500 not in [0x5000,0x6000), 0x7500>0x5000 → go RIGHT. Step2: VMA_D, 0x7500 in [0x7000,0x8000)? YES → FOUND. 2 steps.
B4f. RBTREE BALANCING: RED/BLACK coloring rules ensure depth ≤ 2×log₂(N+1). N=1000 → max depth ≤ 20.
B4g. RBTREE PROBLEM: Binary (2-way), 3 cache lines per node (~40 bytes spread), high mmap_sem contention.
B4h. OLD STRUCT: `vm_area_struct` had: `struct rb_node vm_rb` (embedded), `vm_next`/`vm_prev` (linked list for sequential traversal).
B4h1. EMBEDDED MEANS: rb_node struct (24 bytes) lives INSIDE vm_area_struct, not pointed to. rb_node = {__rb_parent_color (8 bytes), rb_right (8 bytes), rb_left (8 bytes)}.
B4h2. MEMORY LAYOUT: vm_area_struct[offset 0]=vm_start, [offset 8]=vm_end, [offset ~40]=vm_rb.rb_parent_color, [offset ~48]=vm_rb.rb_right, [offset ~56]=vm_rb.rb_left.
B4h3. rb_entry MACRO: Given rb_node pointer, get VMA: VMA_ptr = rb_node_ptr - offset_of(vm_rb) = rb_node_ptr - 40 (example). Reads VMA fields at calculated address.
B4h4. COLOR BIT TRICK: Pointers 8-byte aligned → low 3 bits = 0. Bit 0 stores RED(0) or BLACK(1). Parent =__rb_parent_color & ~3.
B4i. OLD FUNCTION: `find_vma(mm, addr)` did: node=mm->mm_rb.rb_node; while(node) { if addr<vm_start go left; else if addr>=vm_end go right; else return vma; }.
B4j. COMPARISON TABLE: | Property | RB-Tree | Maple | | Branching | 2 | 16 | | Depth/1000 VMAs | ~10 | ~3 | | RAM reads | ~10 | ~3 |
B4k. MAPLE TREE FIX (kernel 6.1, Oct 2022): 16-way branching, 256 bytes/node, RCU-safe. YOUR KERNEL: 6.14.0-37-generic (has Maple Tree ✓).
B4l. REMOVAL: vm_next/vm_prev linked list removed. Now use ma_state + vma_iterator.
B5. YOUR MACHINE CACHE (LIVE DATA):
B5a. CPU: AMD Ryzen 5 4600H. L1d=192 KiB (6 instances), L2=3 MiB, L3=8 MiB. (SOURCE: lscpu)
B5b. CACHE LINE SIZE: 64 bytes. (SOURCE: getconf LEVEL1_DCACHE_LINESIZE = 64)
B5c. TLB SIZE: 3072 × 4K pages = 3072 entries. (SOURCE: /proc/cpuinfo TLB size)
B5d. MAPLE NODE = 256 bytes. CALCULATION: 256 / 64 = 4 cache lines per node.
B5e. CACHE READ OFFSETS: pivot[0] at offset 8 → cache line 0 (0-63). slot[0] at offset 128 → cache line 2 (128-191).
B5f. MAPLE 3 levels: 3 nodes × 4 cache lines = 12 cache line fetches max.
B5g. RB-TREE 10 levels: 10 VMAs × 4 cache lines each = 40 cache line fetches max.
B5h. RESULT: MAPLE = 12 vs RB-TREE = 40. MAPLE is 3.3× fewer cache fetches ✓.
B5i. TLB: MAPLE 3 levels = 3 TLB entries. RB-TREE 10 levels = 10 TLB entries. MAPLE = 3× fewer TLB misses ✓.
B6. AXIOM: Tree has ROOT pointer (ma_root) pointing to first node.
B7. AXIOM: Nodes contain PIVOTS (boundaries) and SLOTS (child pointers or VMA pointers).
B8. AXIOM: LEAF nodes store actual data (VMA pointers). INTERNAL nodes store child pointers.
B9. LOOKUP ALGORITHM: Start at root, compare address to pivots, follow matching slot, repeat until leaf, return VMA.
B10. EXAMPLE: 3 VMAs need 3 slots. 1 node with 16 slots can hold up to 16 VMAs. Tree depth = 1.
B11. EXAMPLE: 100 VMAs need multiple nodes. Tree depth = 2 or 3. Still O(log N) lookups.
INITIALIZATION:
A0a. WHO INITIALIZED mm->mm_mt? fork() → kernel_clone() → copy_mm() → dup_mm() → mm_init() (fork.c:1260).
A0b. mm_init calls mt_init_flags(&mm->mm_mt, MM_MT_FLAGS). (SOURCE: fork.c:1260)
A0c. mt_init_flags (maple_tree.h:772-778) does: mt->ma_flags = flags (line 774), spin_lock_init(&mt->ma_lock) (line 776), rcu_assign_pointer(mt->ma_root, NULL) (line 777).
A0d. DERIVATION: At process creation, ma_root = NULL (tree empty). WITHOUT THIS, ma_root = garbage → crash.
A1. AXIOM: After mmap, kernel inserts VMA into Maple Tree.
A2. AXIOM: Maple Tree stores root pointer in ma_root. (SOURCE: maple_tree.h:225)
A3. AXIOM: Maple nodes are 256-byte aligned. (SOURCE: maple_tree.h:271)
A4. DERIVATION: 256 = 0x100. Low 8 bits of pointer always 0.
A5. AXIOM: Kernel uses low bits to encode node TYPE. (SOURCE: maple_tree.h:67-70)
A5a. WHY SHIFT=3? AXIOM: Bits 0-1 reserved for root indicator. (SOURCE: maple_tree.h:67 "0x??1: Root")
A5b. WHY SHIFT=3? AXIOM: Bit 2 reserved. (SOURCE: maple_tree.h:94 "bit 2 is reserved")
A5c. WHY SHIFT=3? DERIVATION: Bits 3-6 available for type. 4 bits = values 0-15.
A5d. WHY SHIFT=3? DERIVATION: enum maple_type has 4 values (0,1,2,3). Fits in 4 bits. ✓
A5e. WHY SHIFT=3? BINARY EXAMPLE: 0x10 = 0001 0000, bit 4 set, shift >>3 = 0000 0010 = 2.
A6. AXIOM: MAPLE_NODE_TYPE_SHIFT = 0x03. (SOURCE: maple_tree.h:180)
A7. AXIOM: MAPLE_NODE_TYPE_MASK = 0x0F. (SOURCE: maple_tree.h:179)
A8. AXIOM: enum maple_type at maple_tree.h:144-149: maple_dense=0, maple_leaf_64=1, maple_range_64=2, maple_arange_64=3.
A9. AXIOM: VMA tree uses maple_range_64 (Type 2). (SOURCE: mm_types.h:1020 MM_MT_FLAGS)
A9a. WHY maple_range_64? AXIOM: VMAs are RANGES [vm_start, vm_end). Need boundary-based lookup.
A9b. STRUCT: maple_range_64 (maple_tree.h:103-113) has: parent (8 bytes), pivot[15] (15*8=120 bytes), slot[16] (16*8=128 bytes) = 256 bytes total.
A9c. DERIVATION: MAPLE_RANGE64_SLOTS = 16 (maple_tree.h:30). pivot count = 16-1 = 15.
A9d. HOW LOOKUP WORKS: pivot[0]=end of range 0, slot[0]=VMA for range 0. Query: addr <= pivot[0]? If YES, return slot[0].
A9e. WHY NOT maple_dense? Not enough slots for many VMAs.
A9f. WHY NOT maple_leaf_64? Stores values directly, not pointers to structs.
A9g. WHY NOT maple_arange_64? Tracks gaps (for allocation), not needed for simple lookups.
A10. CALCULATION: Encoded type = Type << SHIFT = 2 << 3 = 16 = 0x10.
A11. [SIMULATED] Node allocated at address 0xffff888200000000. (WHY: Kernel addr >= 0xffff800000000000, last 8 bits = 0 for 256-byte alignment. TO GET REAL: Add `pr_info("ma_root=%px", mm->mm_mt.ma_root)` to probe0_driver.c)
A12. CALCULATION: ma_root = node_address | encoded_type = 0xffff888200000000 | 0x10 = 0xffff888200000010.
A13. AXIOM: To decode, function mte_node_type() uses: (entry >> 3) & 0xF. (SOURCE: lib/maple_tree.c:223)
A14. CALCULATION: (0x10 >> 3) & 0xF = 2.
A15. VERIFY: 2 = maple_range_64 ✓.
A16. AXIOM: To get node address: entry & ~0xFF.
A17. CALCULATION: 0xffff888200000010 & 0xFFFFFFFFFFFFFF00 = 0xffff888200000000.
∴ ma_root = 0xffff888200000010 encodes (node_address=0xffff888200000000, type=maple_range_64).

5. DRAW Leaf Node at 0xffff888200000000: `+--maple_range_64 @ 0xffff888200000000--+` `| pivot[0] = 0x78d7ce727fff             |` `| pivot[1] = 0x0 (unused)               |` `| slot[0]  = 0xffff8881abcd0000 (VMA)   |` `| slot[1]  = NULL                       |` `+---------------------------------------+` → VERIFY: pivot[0]=vm_end-1=0x78d7ce728000-1=0x78d7ce727fff ✓.

6. FAULT ADDRESS CHECK: faulting_addr=0x78d7ce727100, pivot[0]=0x78d7ce727fff, compare: 0x78d7ce727100 <= 0x78d7ce727fff → result=TRUE ✓ → ∴ slot[0] matches.

7. CALCULATE offset into VMA: faulting_addr=0x78d7ce727100, vm_start=0x78d7ce727000, offset=0x78d7ce727100-0x78d7ce727000=0x100=256 bytes → VERIFY: strcpy wrote at base+0x100 ✓.

8. RANGE MEMBERSHIP TEST: is 0x78d7ce727100 in [0x78d7ce727000, 0x78d7ce728000)? lower_bound: 0x78d7ce727100 >= 0x78d7ce727000 → 0x100 >= 0x0 → TRUE ✓, upper_bound: 0x78d7ce727100 < 0x78d7ce728000 → 0x100 < 0x1000 → TRUE ✓ → ∴ address in VMA range ✓.

9. WALK ALGORITHM TRACE (STEP-BY-STEP CPU/RAM READS):
W1. INPUT: faulting_addr = 0x78d7ce727100 (from CR2 register, page fault triggered by strcpy).
W2. FUNCTION CALL: mas_walk(&mas) called from lock_vma_under_rcu (mm/memory.c:5712).
W3. READ RAM[&mm->mm_mt.ma_root]: CPU reads 8 bytes from mm_struct. VALUE = 0xffff888200000010.
W4. DECODE ROOT: node_addr = 0xffff888200000010 & 0xFFFFFFFFFFFFFF00 = 0xffff888200000000.
W5. DECODE TYPE: type = (0x10 >> 3) & 0xF = 2 (maple_range_64).
W6. READ RAM[node_addr + 8 + 0*8]: Read pivot[0] from node. OFFSET = parent(8) + pivot_index*8 = 8 + 0 = 8.
W7. VALUE: pivot[0] = 0x78d7ce727fff (this is vm_end - 1 of the first VMA).
W8. COMPARE: Is faulting_addr (0x78d7ce727100) <= pivot[0] (0x78d7ce727fff)?
W9. CALCULATION: 0x78d7ce727100 - 0x78d7ce727fff = 0x78d7ce727100 - 0x78d7ce727fff. 0x100 vs 0xfff. 0x100 < 0xfff → 0x78d7ce727100 < 0x78d7ce727fff → TRUE.
W10. MATCH: Condition TRUE → index = 0. VMA is in slot[0].
W11. READ RAM[node_addr + 8 + 15*8 + 0*8]: Read slot[0]. OFFSET = parent(8) + pivots(15*8=120) + slot_index*8 = 8 + 120 + 0 = 128.
W12. VALUE: slot[0] = 0xffff8881abcd0000 (pointer to vm_area_struct).
W13. RETURN: mas_walk returns 0xffff8881abcd0000.
W14. VERIFY VMA RANGE: Read vma->vm_start at [0xffff8881abcd0000 + 0] = 0x78d7ce727000. Read vma->vm_end at [0xffff8881abcd0000 + 8] = 0x78d7ce728000.
W15. RANGE CHECK: Is 0x78d7ce727100 >= 0x78d7ce727000? YES. Is 0x78d7ce727100 < 0x78d7ce728000? YES. ∴ VMA FOUND ✓.
TOTAL RAM READS: 4 (ma_root, pivot[0], slot[0], vma fields). TOTAL COMPARISONS: 1.

---

## USER CALCULATIONS (Do by hand)

Q1. Calculate vm_end if vm_start=0x55d310000000, size=8192: _______ (Answer: 0x55d310002000)

Q2. Given pivot[0]=0x7f1234567fff, is addr=0x7f1234568000 in range? _______ (Answer: NO, 0x7f1234568000 > 0x7f1234567fff)

Q3. Decode ma_root=0xffff888300000002: node_ptr=_______ type_bits=_______ (Answer: 0xffff888300000000, 0x2)

Q4. Given faulting_addr=0x78d7ce727abc, vm_start=0x78d7ce727000, calculate offset: _______ (Answer: 0xabc=2748 bytes)

---

## FAILURE PREDICTIONS

F1. ma_root=NULL → mas_walk returns NULL → goto inval → lock_vma_under_rcu returns NULL → do_user_addr_fault takes lock_mmap path.

F2. addr < vm_start → range check fails (line 5720) → goto inval_end_read → release lock → return NULL.

F3. addr >= vm_end → range check fails (line 5720) → goto inval_end_read → release lock → return NULL.

F4. vma->detached=1 → VMA was isolated during walk → vma_end_read → goto retry → restart walk.

F5. atomic_cmpxchg fails (lock already held) → vma_start_read returns 0 → goto inval → return NULL.

---

## NEW THINGS INTRODUCED WITHOUT DERIVATION: None. All addresses derived from: CR2=0x78d7ce727100 (hardware), vm_start from mmap return, pivot from vm_end-1, slot from VMA allocation
