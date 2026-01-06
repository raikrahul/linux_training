# PAGE FAULT OBSERVATION: TASK GRILLING DOCUMENT

## GOAL STATEMENT
Observe kernel page fault handling in real-time while userspace code is halted in GDB.

## SUB-TASK BREAKDOWN

### TASK A: HALT USERSPACE AT CORRECT MOMENT
:QUESTIONS:
A1. Where exactly to set breakpoint? Before mmap? After mmap? At the write instruction?
A2. If breakpoint is BEFORE the write, does the PTE exist yet? (NO, mmap only creates VMA)
A3. If breakpoint is AFTER the write, page fault already happened. Too late?
A4. How to break AT the instruction that causes fault without triggering fault first?
A5. Can GDB single-step INTO a page fault? (NO, page fault is hardware exception)
A6. What GDB command shows if current instruction will fault?

### TASK B: GET PID FROM HALTED PROCESS
:QUESTIONS:
B1. If process is halted in GDB, is it still visible in /proc/PID?
B2. Can we run shell commands while GDB is halted? (YES, separate terminal)
B3. How to find PID? `pgrep mm_exercise_user`? Or use GDB's `info proc`?
B4. Is PID stable between runs? (NO, different each time)
B5. How to pass PID to kernel module? ioctl? proc file? module parameter?

### TASK C: KERNEL OBSERVES PRE-FAULT STATE
:QUESTIONS:
C1. How does kernel module know which PID to watch?
C2. How does kernel get the mm_struct for a userspace PID? `find_get_task_by_vpid()`?
C3. Once we have mm_struct, how to find the VMA for our address?
C4. How to walk page table BEFORE fault? Will PTE exist? (NO)
C5. What does "PTE not present" look like in pagewalk? pte_none()? pte_present()?
C6. Can we install a kprobe on `handle_mm_fault`? What data do we capture?

### TASK D: TRIGGER FAULT FROM USERSPACE
:QUESTIONS:
D1. How to "continue" in GDB to trigger exactly ONE instruction (the faulting write)?
D2. Will GDB stop after the fault? Or after the write completes?
D3. How long does page fault take? Microseconds? Can we observe it in real-time?
D4. If kernel allocates page, does it zero the memory? When?
D5. How to see page allocation in kernel? kprobe on `alloc_pages()`?

### TASK E: KERNEL OBSERVES POST-FAULT STATE
:QUESTIONS:
E1. After fault, is PTE now present? How to verify?
E2. What is the PFN? Is it same as what pagemap reports?
E3. Can we read the 0xAA byte from kernel side using kmap()?
E4. How to correlate kernel observations with GDB output?
E5. Timing: Will kprobe fire BEFORE GDB resumes or AFTER?

### TASK F: SYNCHRONIZATION PROBLEM
:QUESTIONS:
F1. GDB halts userspace. Kernel is STILL RUNNING. How to "pause" kernel observation?
F2. If we use kprobe on handle_mm_fault, it fires when ANY process faults. How to filter?
F3. Race condition: User continues in GDB → fault happens → kprobe fires → how to notify user?
F4. Can we make kernel module "wait" until signaled? (Dangerous: blocking in kernel)
F5. Alternative: Log everything to ring buffer, dump after?

### TASK G: OUTPUT/VISUALIZATION
:QUESTIONS:
G1. How to show kernel output while GDB is running? Separate terminal with `dmesg -w`?
G2. How to correlate timestamps between GDB and dmesg?
G3. Can we print VMA state before and after? What fields are interesting?
G4. Can we print page table entries (raw hex) before and after?

## EDGE CASES TO CONSIDER
E1. What if page is already faulted in from previous run? (Use fresh mmap each time)
E2. What if kernel decides to prefetch adjacent pages? (Possible, depends on fault behavior)
E3. What if ASLR changes addresses? (Record address in each run)
E4. What if process dies while GDB is attached? (Cleanup handling)

## CHAIN OF EVENTS (EXPECTED)
1. Userspace: mmap() → VMA created, PTE empty.
2. GDB: Breakpoint hit BEFORE write instruction.
3. Shell: Get PID, pass to kernel module.
4. Kernel Module: Walk page table, confirm PTE empty, install kprobe.
5. GDB: Single-step (continue).
6. CPU: Execute write → #PF exception.
7. Kernel: handle_mm_fault() → kprobe fires → log to dmesg.
8. Kernel: Allocate page → update PTE → return.
9. CPU: Retry write → success.
10. GDB: Stops at next instruction.
11. Kernel Module: Walk page table again, confirm PTE present, log PFN.

## UNSOLVED PROBLEMS
U1. How to make kprobe fire ONLY for our PID, not all processes?
U2. How to "pause" between step 8 and step 9 to inspect intermediate state?
U3. How to guarantee the exact PFN is observable before process continues?

## NEW THINGS INTRODUCED WITHOUT DERIVATION: ___
- kprobe: Not yet derived. MUST DERIVE BEFORE USE.
- find_get_task_by_vpid: Not yet derived. MUST DERIVE BEFORE USE.
- handle_mm_fault: Not yet derived. MUST DERIVE BEFORE USE.
- kmap: Not yet derived. MUST DERIVE BEFORE USE.

### TASK H: PAGE TABLE WALKER VERIFICATION (AXIOM PROOF)
:QUESTIONS:
H1. User calculates PGD Index = 0xAE. How to prove it?
H2. Need kernel module to walk tables and print indices.
H3. Solution: Add `MM_AXIOM_IOC_WALK_VA` to `mm_exercise_hw.c`.
H4. Input: PID, VA. Output: Dmesg log with indices at each level.
