# PROBE 0 CALIBRATION WORKSHEET

## TERMINAL 1: USERSPACE TRIGGER
1.  **Run**: `./mm_exercise_user`
2.  **Observe**:
    - `PID: 78486` (Example)
    - `VA: 0x765a7dc3b000` (Example)
3.  **Calculate**:
    - Fault Address = `VA + 0x100` = `0x765a7dc3b100`

## TERMINAL 2: KERNEL DRIVER
1.  **Command**:
    ```bash
    sudo insmod kprobe_driver.ko target_pid=78486 target_addr=0x765a7dc3b100
    ```
2.  **Wait**: 10 seconds.
3.  **Capture**:
    ```bash
    sudo dmesg | grep "AXIOM_TRACE"
    ```
    → Expected: `RCV_ADDR = 0x765a7dc3b100`

## THE BLACKLIST TRAP (Axiom)
- `exc_page_fault` -> **NOT PROBEABLE** (Kernel security blacklist).
- `lock_vma_under_rcu` -> **PROBEABLE** (First function receiving the address).
