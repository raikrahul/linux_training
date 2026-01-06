# AXIOM: THE FIRST REACHABLE RECEIVER

## 1. THE CALL CHAIN (arch/x86/mm/fault.c)
1.  **HW Entry**: `asm_exc_page_fault` -> `exc_page_fault`
2.  **State**: `exc_page_fault` reads `CR2` into `address` variable.
3.  **Blacklist Audit**:
    - [X] `exc_page_fault`: **BLACKLISTED** (`-EINVAL`)
    - [X] `handle_page_fault`: **INLINED** (No separate symbol)
    - [X] `do_user_addr_fault`: **BLACKLISTED** (`-EINVAL`)
    - [ ] `lock_vma_under_rcu`: **REACHABLE** ✓

## 2. THE REACHABILITY PROOF
*   **Search**: `cat /sys/kernel/debug/kprobes/blacklist`
*   **Result**: `do_user_addr_fault` exists at `0xffffffff8df56490`.
*   **Search**: `perf_sw_event` (Line 1280)
*   **Result**: General performance counter, not fault-specific.
*   **Search**: `lock_vma_under_rcu` (Line 1316)
*   **Result**: First dedicated fault-logic function that receives `address` as Arg 2.

## 3. PHYSICAL VERIFICATION
*   **Kallsyms Check**: `lock_vma_under_rcu` has address in `mm/memory.c`.
*   **Blacklist Check**: NOT present in blacklist.
∴ **lock_vma_under_rcu** is the first function to kprobe to see the faulting address.
