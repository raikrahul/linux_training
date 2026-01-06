# THE JOURNEY OF TWO INTEGERS

## INTEGER 1: THE ADDRESS (0x78d7ce727100)

### t=0 (Hardware)

CR2 register = 0x78d7ce727100 (8 bytes in silicon). CPU writes this during page fault exception. IDT entry 14 invoked.

### t=1 (asm_exc_page_fault)

read_cr2() returns 0x78d7ce727100. Copied to stack variable `address` (8 bytes at stack offset +16).

### t=2 (exc_page_fault)

`address` parameter = 0x78d7ce727100. Passed via C calling convention (on stack, not register).

### t=3 (handle_page_fault, inlined)

Checks: address < 0x00007fffffffffff? YES (0x78d7ce727100 < 0x00007fffffffffff). Dispatches to do_user_addr_fault.

### t=4 (do_user_addr_fault entry)

`address` parameter (stack) = 0x78d7ce727100. Used for vsyscall check (lines 1307-1310): address == 0xffffffffff600000? NO.

### t=5 (lock_vma_under_rcu call, Probe 0)

RSI register loaded with 0x78d7ce727100 (ABI Arg2). RDI = mm pointer. KPROBE FIRES.

### t=6 (lock_vma_under_rcu, MA_STATE)

`address` = 0x78d7ce727100. Maple tree indexes BY address. mas.index = 0x78d7ce727100, mas.last = 0x78d7ce727100.

### t=7 (mas_walk)

Searches mm->mm_mt for VMA where: vma->vm_start <= 0x78d7ce727100 < vma->vm_end. Returns vma = 0xffff888... (kernel pointer).

### t=8 (Range check)

vma->vm_start = 0x78d7ce711000 (assumed). vma->vm_end = 0x78d7ce728000. Check: 0x78d7ce711000 <= 0x78d7ce727100 < 0x78d7ce728000. TRUE.

### t=9 (Return to do_user_addr_fault)

vma != NULL. Continues to handle_mm_fault.

### t=10 (handle_mm_fault, Probe 1)

RSI register = 0x78d7ce727100. Arg2 to handle_mm_fault.

### FINAL STATE

Address 0x78d7ce727100 unchanged through entire path. Used for: VMA lookup, Permission check, Page table walk (next steps).

---

## INTEGER 2: THE FLAGS (6 → 0x41 → 0x1041)

### t=0 (Hardware)

Error Code pushed to stack by CPU = 6 (0x6, binary 110). Bit 0 (P)=0, Bit 1 (W/R)=1, Bit 2 (U/S)=1.

### t=1 (exc_page_fault)

`error_code` parameter = 6. Passed to handle_page_fault.

### t=2 (handle_page_fault, inlined)

Passed to do_user_addr_fault unchanged.

### t=3 (do_user_addr_fault entry)

`error_code` = 6 (unsigned long). Local variable `flags` initialized = FAULT_FLAG_DEFAULT = 0.

### t=4 (User mode check, line 1272)

user_mode(regs): CS=0x33, (0x33 & 3)=3. TRUE.

### t=5 (Flag OR, line 1274)

flags |= FAULT_FLAG_USER. 0 | 0x40 = 0x40.

### t=6 (Write check, line 1290)

error_code & X86_PF_WRITE: 6 & 2 = 2 (TRUE).

### t=7 (Flag OR, line 1291)

flags |= FAULT_FLAG_WRITE. 0x40 | 0x1 = 0x41.

### t=8 (Instruction check, line 1292)

error_code & X86_PF_INSTR: 6 & 16 = 0 (FALSE). No change.

### t=9 (handle_mm_fault call, line 1324)

flags | FAULT_FLAG_VMA_LOCK: 0x41 | 0x1000 = 0x1041. RDX register loaded with 0x1041.

### FINAL STATE

Hardware Error Code 6 transformed to Software Flags 0x1041 through bitwise ORs. Each bit has architectural meaning:

- Bit 0 (WRITE): Set because Error Code bit 1 was set.
- Bit 6 (USER): Set because CS register had RPL=3.
- Bit 12 (VMA_LOCK): Set because lock_vma_under_rcu succeeded.

---

## AXIOM: TWO INTEGERS, ENTIRE SUBSYSTEM

Address determines WHICH memory object (VMA).
Flags determine HOW to handle (Write vs Read, User vs Kernel, Retry vs Kill).
Both flow through 20+ function calls without mutation (address) or with controlled derivation (flags).

## ERROR AUDIT: CROSS-CONVERSATION PATTERN

### PATTERN 1: Address Space Confusion
Conversations: "Understanding Userspace Memory Maps", Current
Error: Conflated "User Address Space" (VA range) with "User Mode" (CPU privilege).
Recurrence: 3 times across 2 conversations.
Root: Vocabulary collision ("user" has 2 meanings).
Fix: Always qualify: "User Address SPACE" vs "User EXECUTION Mode".

### PATTERN 2: Flags Abstraction Leap
Conversations: "Axiomatic Flags Derivation", Current
Error: Skipped derivation of WHY architecture translation exists.
Recurrence: 2 times.
Root: Assumed portability is obvious.
Fix: State axiom: "Linux runs on 10+ architectures. One `mm/` codebase."

### PATTERN 3: Interrupt State Ignorance
Conversations: Current
Error: Did not consider RFLAGS bit 9 preservation during exceptions.
Recurrence: 1 time.
Root: Focused on address flow, ignored CPU state.
Fix: Axiom: "Exceptions save ALL CPU state in pt_regs."

### PATTERN 4: Control Flow Misreading
Conversations: Current
Error: Did not recognize `else` block as kernel mode path.
Recurrence: 1 time.
Root: if/else visual scan failure.
Fix: Annotate: "if=UserMode, else=KernelMode" when reviewing.

### PATTERN 5: Scope Boundary Violation
Conversations: "Kernel Page Metadata Axioms", Current
Error: Assumed `do_user_addr_fault` excludes kernel execution.
Recurrence: 2 times across 2 conversations.
Root: Function naming convention misinterpretation.
Fix: Axiom: "Function scope = PARAMETER TYPE, not CALLER CONTEXT."

### META-ERROR: Derivation Gap
Total errors: 7 across Current conversation.
Cause: Introduced terms without line-by-line buildup.
Prevention: Every new word requires: Problem → Definition → Derivation → Example.

