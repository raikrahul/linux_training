:AXIOM R1: LOCK_MM AND FIND VMA ARGS
01. Function: `lock_mm_and_find_vma(mm, addr, regs)`.
02. Arg `regs`: Pointer to `struct pt_regs` (CPU Registers at Fault).
03. Usage A: `get_mmap_lock_carefully(mm, regs)`.
04. Logic: If acquiring lock FAILS (would block), we must handle it.
05. Check: `if (regs && !user_mode(regs))`.
06. Case 1 (User Fault): `user_mode(regs)` is True. Thread can sleep. 
    Action: Wait for lock (using `mmap_read_lock_killable`).
07. Case 2 (Kernel Fault): `user_mode(regs)` is False. Thread CANNOT sleep?
    Action: Check Exception Tables (`search_exception_tables(ip)`).
    Why? If kernel Code faulted, it might be a "safe" fault (like `get_user`).
    But if it's a BUG (deadlock potential), we must NOT sleep waiting for lock.

:AXIOM R2: ADDRESS ARGUMENT
08. Arg `addr`: The Virtual Address that caused the fault (from CR2).
09. Usage: `find_vma(mm, addr)`.
10. Goal: Find the VMA containing this address.

:CONCLUSION (WHY REGS?)
11. `regs` tells us: "Did this fault happen in User Mode or Kernel Mode?"
12. This determines: "Can we safely sleep waiting for the `mmap_sem` lock?"
13. User Mode → Yes. Kernel Mode → Maybe/No (Deadlock risk).
