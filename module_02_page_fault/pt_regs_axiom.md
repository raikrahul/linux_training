:AXIOM P1: STRUCT PT_REGS DEFINITION
01. Definition: `arch/x86/include/asm/ptrace.h:103`.
02. Members: r15..r8, ax..di (Callee-clobbered), orig_ax, ip, cs, flags, sp, ss.
03. Purpose: Stores CPU state at the moment of interrupt/exception.

:AXIOM P2: WHO POPULATED IT? (Time T0)
04. Event: Hardware raises Exception #14 (#PF).
05. Hardware Action (CPU Microcode):
    -   Switch stack to Kernel Stack (TSS.SP0).
    -   Push `SS` and `RSP` (User Stack Pointer).
    -   Push `RFLAGS`.
    -   Push `CS` and `RIP` (Instruction Pointer).
    -   Push `Error Code`.
    -   Jump to IDT Handler (`asm_exc_page_fault`).

:TRACE 1: KERNEL ENTRY (Time T0 + 1 cycle)
06. Handler: `asm_exc_page_fault` (arch/x86/entry/entry_64.S).
07. Macro: `DEFINE_IDTENTRY_RAW_ERRORCODE` calls ASM stubs.
08. Instruction: `PUSH_AND_CLEAR_REGS` (entry_64.S).
    -   Pushes `RAX, RCX, RDX, RSI, RDI, R8-R11`.
    -   Pushes `RBX, RBP, R12-R15`.
09. This Stack Frame IS `struct pt_regs`.
10. The pointer `struct pt_regs *regs` passed to C functions is literally `%rsp`.

:CONCLUSION (WHO MADE IT?)
11. Hardware: Pushed critical 6 (SS, SP, FLAGS, CS, IP, ERR).
12. Software (ASM): Pushed remainders (GPRs).
13. Together they form `struct pt_regs` on the Kernel Stack. ✓
