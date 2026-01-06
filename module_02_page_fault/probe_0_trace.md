
# PROBE 0 TRACE: lock_vma_under_rcu

## 1. FAILURE AXIOMS (The Blacklist)

* **Target 1**: `exc_page_fault`
  * Source: `arch/x86/mm/fault.c:1527`
  * Check: `grep exc_page_fault /sys/kernel/debug/kprobes/blacklist` → Match Found ✓
  * Result: `register_kprobe` → `-EINVAL` (22) → ✗ FAIL
* **Target 2**: `do_user_addr_fault`
  * Source: `arch/x86/mm/fault.c:1255`
  * Check: `grep do_user_addr_fault /sys/kernel/debug/kprobes/blacklist` → Match Found ✓
  * Result: ✗ FAIL
* **Target 3**: `handle_page_fault`
  * Check: `grep handle_page_fault /proc/kallsyms` → Match Per Line Empty → ✗ FAIL
  * Logic: Function is `static` + `inline` → Compiler absorbed bytes into caller → No jump target exists.

## 2. SUCCESS AXIOMS (The Reachable Entry)

* **Target**: `lock_vma_under_rcu`
  * Check: `grep lock_vma_under_rcu /proc/kallsyms` → `0xffffffff8df55220 T lock_vma_under_rcu` ✓
  * Check: `grep lock_vma_under_rcu /sys/kernel/debug/kprobes/blacklist` → 0 Match → ✓ SUCCESS
  * Logic: Function exists in RAM + Not banned → Probe reachable.

## 3. DATA DERIVATION (Input → Output)

* **Input 1**: `VA_BASE` = `0x78d7ce727000` (from userspace log)
* **Input 2**: `OFFSET` = `0x100` (from `strcpy` logic)
* **Computation**: `0x78d7ce727000` + `0x100` = `0x78d7ce727100`
* **Target VA**: `0x78d7ce727100`

## 4. LIVE VERIFICATION (Brute Force Proof)

* **PID**: `86356`
* **Command**: `insmod probe0_driver.ko pid_filter=86356 addr_filter=0x78d7ce727100`
* **dmesg**: `[18374.125551] PROBE_0_HIT: PID=86356 ADDR=0x78d7ce727100`
* **Comparison**: `0x78d7ce727100` == `0x78d7ce727100` → ✓ TRUE
∴ **lock_vma_under_rcu** is verified as the first reachable receiver of the hardware address.

---
TERMS INTRODUCED WITHOUT DERIVATION: None.
