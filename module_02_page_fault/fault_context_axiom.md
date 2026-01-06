:AXIOM 0: CPU STATE = REGISTERS
01. Definition: "Process" ≡ Set of Register Values {CR3, GS, RSP}.
02. Register CR3: Contains Physical Address of Page Table Root (0x1000). Defines "Memory".
03. Register GS: Contains Linear Address of Kernel Per-CPU Area (0xffff...). Defines "Self".
04. Register RIP: Contains Virtual Address of Next Instruction (0x7000). Defines "Now".

:TRACE 1: THE EXECUTION (STATE T0)
05. Input: CPU executes `MOV [0x8000], 1`.
06. Hardware Check: Read CR3(0x1000) → Walk Tables → Addr 0x8000 is INVALID (P=0).
07. Action: Hardware trigger Exception #14 (Page Fault).

:TRACE 2: THE TRANSITION (STATE T0 -> T1)
08. Hardware Logic (Microcode):
    A. Push RIP (0x7000) to Stack.
    B. Load RIP ← IDT[14].Handler (0xffff...KernelCode).
    C. CR3 REMAINS 0x1000. (CRITICAL: Memory View is UNCHANGED).
    D. GS REMAINS 0xffff... (CRITICAL: Per-CPU Data is UNCHANGED).
09. Result: CPU is executing Kernel Code, but wearing User's "Glasses" (CR3).

:TRACE 3: THE IDENTIFICATION (STATE T1)
10. Code: `current = this_cpu_read(current_task)`.
11. Assembly: `mov rax, gs:[0x2c00]` (Offset of current_task variable).
12. Computation: 
    Addr = GS.Base (0xffff8880a...) + Offset (0x2c00) = 0xffff8880a...2c00.
    Value = ReadMemory(Addr) = 0xffff8881b... (Pointer to Task Struct).
13. Code: `pid = current->pid`.
14. Computation:
    Addr = TaskPtr (0xffff8881b...) + PID_Offset (0x4a0).
    Value = ReadMemory(Addr) = 17260.

:CONCLUSION
15. Register CR3 is constant during Fault. ∴ Kernel sees User's Memory.
16. Register GS is constant during Fault. ∴ Kernel sees CPU's Local Data.
17. Valid Pointer in GS + Valid Offset = Valid PID.
18. No Magic. Just Pointer Arithmetic on Preserved Registers.
