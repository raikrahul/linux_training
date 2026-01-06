:AXIOM F1: FLAG ORIGIN (Time T0)
01. Hardware pushes `error_code` to stack (Exception #14).
02. `error_code` contains: Bit 0 (P), Bit 1 (W/R), Bit 2 (U/S), Bit 4 (I/D).
03. Source: CPU Manual Vol 3, Sec 4.7.

:TRACE 1: SOFTWARE TRANSLATION (Time T1)
04. Function `do_user_addr_fault` (arch/x86/mm/fault.c:1199).
05. Init: `unsigned int flags = FAULT_FLAG_DEFAULT;` (Line 1207).
06. Input: `error_code` from hardware.
07. Translation:
    - Line 1272: If `user_mode(regs)` → `flags |= FAULT_FLAG_USER`.
    - Line 1290: If `error_code & X86_PF_WRITE` → `flags |= FAULT_FLAG_WRITE`.
    - Line 1292: If `error_code & X86_PF_INSTR` → `flags |= FAULT_FLAG_INSTRUCTION`.

:CONCLUSION (WHO SUPPLIED?)
08. Hardware supplied raw `error_code`.
09. Kernel function `do_user_addr_fault` converted it to `flags`.
10. Then passed to `handle_mm_fault`.
