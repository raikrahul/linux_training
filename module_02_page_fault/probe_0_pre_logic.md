
# AXIOMATIC EXPLANATION: PRE-LOCK LOGIC

## 1. Interrupts Enabled (Line 1273)
*   **Logic**: `local_irq_enable()`
*   **Why**: Page faults are slow (disk I/O, sleeping locks). We CANNOT hold up the whole CPU. We must let other hardware interrupts (Network, Timer, Mouse) happen while we handle this memory issue.

## 2. Event Counting (Line 1280)
*   **Logic**: `perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS)`
*   **Why**: Statistical visibility. This increments the counter you see in `top` or `vmstat`. Without this, admins are blind to system memory pressure.

## 3. Flag Construction (Line 1290)
*   **Logic**: `flags |= FAULT_FLAG_WRITE`
*   **Why**: The generic memory manager (`handle_mm_fault`) is architecture-independent. It doesn't know x86 hardware codes. We must translate "Hardware Bit 1" into "Generic Flag Bit 0" so the rest of the kernel understands "This is a Write".

## 4. vsyscall Check (Line 1307)
*   **Logic**: `is_vsyscall_vaddr(address)`
*   **Why**: Legacy support. Old binaries use specific high-memory addresses for system calls. These don't have real VMAs (no struct in RAM). We must emulate them manually or the program crashes.

## 5. Kernel Mode Check (Line 1313)
*   **Logic**: `if (!(flags & FAULT_FLAG_USER)) goto lock_mmap`
*   **Why**: Kernel page faults (recursion, bugs) are dangerous. The "Optimized VMA Lock" path is designed for USER space scalability. Kernel faults are rare and complex; force them to the safe, slow path (Global Lock).

## 6. Logic (Error Code Translation)
*   **Variable**: `unsigned long error_code`
    *   **Value**: `6` (0x6)
    *   **Representation**: `0x...0006` (8 bytes)
*   **Derivation**:
    1.  **Bit 1 Checked**: `error_code & X86_PF_WRITE` (Line 1290)
    2.  **Calculation**: `6 & 2 = 2` (Non-Zero)
    3.  **Action**: `flags |= FAULT_FLAG_WRITE` (Line 1291)
    4.  **Result**: The kernel now knows this is a "Write Fault" (Generic) instead of "Page Fault Error 6" (x86 specific).
