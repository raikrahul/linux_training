
# STEP 0 COMPREHENSIVE: THE HARDWARE BRIDGE

## 1. OBJECTIVE

To axiomatically verify the transition of the **Faulting Virtual Address (VA)** from the CPU `CR2` register into the Linux Kernel C logic.

## 2. THE BLACKLIST BATTLE (Why earlier probes failed)

We attempted to probe the raw hardware entry points but faced kernel security restrictions:

* **Target 1**: `exc_page_fault` (The Assembly Wrapper)
  * **Result**: ✗ FAILED.
  * **Reason**: Distinctly listed in `/sys/kernel/debug/kprobes/blacklist`.
  * **Why?**: Interrupt context entry is too fragile for kprobe logic (`int3` recursion risk).
* **Target 2**: `handle_page_fault` (The Dispatcher)
  * **Result**: ✗ FAILED.
  * **Reason**: **INLINED**. The compiler optimized this function away; it has no symbol in the binary (checked via `/proc/kallsyms`).
* **Target 3**: `do_user_addr_fault` (The Kernel Entry)
  * **Result**: ✗ FAILED.
  * **Reason**: Distinctly listed in `/sys/kernel/debug/kprobes/blacklist`.
  * **Why?**: Still considered "Entry Text" (NOKPROBE_SYMBOL) by the kernel developers.

## 3. THE SOLUTION: lock_vma_under_rcu

We identified `lock_vma_under_rcu` as the **First Reachable Receiver**.

* **Location**: `mm/memory.c:5695`
* **Reachable**: ✓ Yes (Not blacklisted, Global Text symbol).
* **Arguments**:
  * `mm` (Arg 1, RDI): The address space.
  * `address` (Arg 2, RSI): The faulting VA.

## 4. AXIOMATIC LOGIC (Why this function?)

This function represents the precise moment logic transitions from **"I have an address"** to **"I am looking for the Memory Object (VMA)"**.

* **Input**: Raw Address (from CR2) + MM Struct (from Task).
* **Operation**: `mas_walk(&mas)` (Line 5703).
* **Significance**: Before this line, the concept of a "VMA" for this fault does **not exist** in the execution stream. This is the **Search Origin**.

## 5. LIVE VERIFICATION (The Proof)

We wrote a dedicated driver (`probe0_driver.c`) to filter by PID and Address.

* **User Action**: Run generic `strcpy` at `Base + 0x100`.
* **Userspace Data**:
  * PID: `86356`
  * VA Base: `0x78d7ce727000`
  * Target: `0x78d7ce727100`
* **Kernel Capture**:
  * `[18374.125551] PROBE_0_HIT: PID=86356 ADDR=0x78d7ce727100`
* **Verification**:
  * User `0x78d7ce727100` == Kernel `0x78d7ce727100`
  * Difference = `0` bits.

## 6. FULL CALL CHAIN (Reconstructed)

1. **Hardware**: CPU Trap 14 (Page Fault) -> Pushes Error Code `6`.
2. **Asm Entry**: `asm_exc_page_fault` -> READs `CR2` (Fault Address).
3. **Route**: `exc_page_fault` -> `handle_page_fault` (Inlined) -> `do_user_addr_fault`.
4. **Derivation**: `do_user_addr_fault` gets `mm = current->mm`.
5. **Pivot**: Calls `lock_vma_under_rcu(mm, address)`.
6. **Capture**: Our Probe 0 hits here.

## 7. CONCLUSION

Step 0 is **Axiomatically Verified**. We have proven that the address causing the crash in userspace is bit-exactly the same address arriving at the VMA lookup logic in the kernel.

---
TERMS INTRODUCED WITHOUT DERIVATION: None.
