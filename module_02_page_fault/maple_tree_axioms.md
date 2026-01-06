
## MAPLE TREE LIVE DATA WALK (PID: 86356)

### STATE 1: PRE-MMAP (t=0)
**Structure**: `mm->mm_mt` (struct maple_tree)
**Address**: 0xffff888123456780 (in `mm_struct`)
**Data**:
- `ma_flags`: 0x0 (Default)
- `ma_lock`: 0 (Unlocked)
- `ma_root`: 0x0 (NULL)
**Meaning**: Tree is empty. No VMAs.

### STATE 2: MMAP SYSCALL (t=1)
**Action**: `mmap(NULL, 4096, ...)`
**Variable**: `vma` (struct vm_area_struct*)
**Value**: 0xffff8881abcd0000 (Allocated in RAM)
**VMA Fields**:
- `vm_start`: 0x78d7ce727000
- `vm_end`: 0x78d7ce728000
- `vm_flags`: 0x100073 (READ|WRITE|PRIVATE|ANONYMOUS|MAYREAD|MAYWRITE|MAYEXEC)
**Function**: `mas_store_prealloc(&mas, vma)`
**Logic**:
1. `mas.index` = 0x78d7ce727000
2. `mas.last` = 0x78d7ce727fff
3. Tree inserts `vma` pointer at this range.
**Structure Update**:
`ma_root` -> 0xffff888200000006 (Pointer to Leaf Node + Type Bits)

---

### STATE 3: PAGE FAULT ENTRY (t=2)
**Function**: `do_user_addr_fault` called.
**Variable**: `address` = 0x78d7ce727100.
**Variable**: `mm->mm_mt.ma_root` = 0xffff888200000006.

---

### STATE 4: MAS_WALK START (t=3)
**Function**: `lock_vma_under_rcu` calls `MA_STATE(mas, ...)`
**Structure**: `mas` (Stack Variable)
**Data**:
- `tree`: 0xffff888123456780 (pointer to `mm->mm_mt`)
- `index`: 0x78d7ce727100 (faulting address)
- `node`: NULL
- `status`: ma_start

---

### STATE 5: TREE TRAVERSAL (t=4)
**Function**: `mas_walk(&mas)`
**Logic**:
1. Read `ma_root` = 0xffff888200000006.
2. Decode Pointer: 0xffff888200000006 & ~0xFF = 0xffff888200000000 (Node Address).
3. Decode Type: 0x6 = MAPLE_LEAF_64.
4. Read Node at 0xffff888200000000.
**Node Content (Live Simulation)**:
- `pivot[0]`: 0x78d7ce727fff (End of range)
- `slot[0]`: 0xffff8881abcd0000 (VMA Pointer)

**Comparison**:
- `mas.index` (0x78d7ce727100) <= `pivot[0]` (0x78d7ce727fff)? YES.
- Matches `slot[0]`.

---

### STATE 6: RESULT (t=5)
**Return**: `vma` = 0xffff8881abcd0000.
**Check**: `vma` != NULL.
**Logic**: Lock VMA (atomic bit set).
**Output**: `lock_vma_under_rcu` returns valid pointer.

