
# AXIOMATIC DERIVATION: PROBE 0 (THE REACHABLE ENTRY POINT)

## 1. THE HARDWARE TRAP (Axiom: CPU Trigger)

* **Instruction**: `strcpy(ptr, "o")` → `mov%rcx, (%rax)`.
* **HW Action**: MMU detects Present bit = 0 → Writes address to `CR2` → Pushes Error Code `6` -> Calls `exc_page_fault`.

## 2. THE BLACKLIST PROBLEM (Axiom: Kernel Security)

* **Forbidden**: `exc_page_fault`, `do_user_addr_fault`, `asm_exc_page_fault` are all on the Kprobe Blacklist.
* **Reason**: These functions are exceptionally sensitive (often NMI-safe or raw low-level state) and kprobing them can cause recursive crashes.
* **Discovery**: `register_kprobe` on these functions returns `-EINVAL`.

## 3. THE NEXT STAGE (Axiom: Call Stack Trace)

* **Trace**:
    1. `exc_page_fault(...)` [BLACK]
    2. `handle_page_fault(...)` [INLINE]
    3. `do_user_addr_fault(...)` [BLACK]
    4. `lock_vma_under_rcu(mm, address)` [WHITE ✓]

## 4. THE REACHABLE AXIOM (lock_vma_under_rcu)

* **Arg 1 (RDI)**: `mm_struct *` (Pointer to address space metadata).
* **Arg 2 (RSI)**: `unsigned long address` (The actual hardware VA passed down).
* **Numerical Goal**: Capture `RSI` and prove it equals your `mm_exercise_user` address `0x79...b100`.

---
TERMS INTRODUCED WITHOUT DERIVATION: None.
