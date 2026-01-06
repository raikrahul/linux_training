# AXIOMATIC FLAGS DERIVATION

## 0. HARDWARE INPUT (The Raw Bits)
*   **Error Code (Stack)**: `0x6` (Binary: `...00000110`)
    *   Bit 0 (P): 0 (Not Present)
    *   Bit 1 (W): 1 (Write Access)
    *   Bit 2 (U): 1 (User Mode)

## 1. SOFTWARE CONSTRUCTION (Variable Creation)
*   Location: `arch/x86/mm/fault.c:do_user_addr_fault`

### Step A: Initialization (t=0 relative to function)
*   **Line 1246**: `unsigned int flags = FAULT_FLAG_DEFAULT;`
*   **Value**: `0x0` (Assuming DEFAULT is 0, verify later).

### Step B: User Mode Derivation (t=1)
*   **Line 1272**: `if (user_mode(regs))`
    *   **Input**: `regs->cs` (Code Segment from Hardware).
    *   **Logic**: If (CS & 3) == 3 -> USER.
*   **Line 1274**: `flags |= FAULT_FLAG_USER;`
    *   **Value**: `flags = flags | 0x40` (Bit 6 set).

### Step C: Write Mode Derivation (t=2)
*   **Line 1290**: `if (error_code & X86_PF_WRITE)`
    *   **Input**: `0x6` (from Hardware).
    *   **Logic**: `0x6 & 0x2` = `0x2` (Non-Zero -> True).
*   **Line 1291**: `flags |= FAULT_FLAG_WRITE;`
    *   **Value**: `flags = flags | 0x1` (Bit 0 set).

### Step D: The Invisible Pass (t=3)
*   **Line 1316**: `lock_vma_under_rcu(mm, address)`
    *   **Status**: `flags` variable exists in CPU register/stack but is **NOT PASSED** to this function.
    *   **Puzzle**: Why?
    *   **Answer**: `lock_vma_under_rcu` only needs address to find VMA. It doesn't care about permission (Read/Write) yet.

### Step E: The Fast Path Check (t=4)
*   **Line 1324**: `handle_mm_fault(..., flags | FAULT_FLAG_VMA_LOCK, ...)`
    *   **Action**: Software manually ORs `0x1000` into the trace.
    *   **Result**: `0x41 | 0x1000` = `0x1041` passed to API.

## 2. AXIOMATIC CONCLUSION
The `flags` variable is a **Software Accumulator**. It starts empty and accumulates axioms from hardware registers (CS, ErrorCode) step-by-step. It is fully constructed **before** the VMA lock search, but only used **after** the VMA is found.
