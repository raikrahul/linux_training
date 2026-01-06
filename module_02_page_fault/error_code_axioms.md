# AXIOM: ERROR CODE POPULATION

## LEVEL 0: HARDWARE PRIMITIVES

### AXIOM 0.1: CPU

Central Processing Unit. Silicon chip. Executes binary instructions. Has internal registers (storage locations).

### AXIOM 0.2: Memory Access Instruction

Example: `mov rax, [0x78d7ce727100]`. CPU reads 8 bytes from address 0x78d7ce727100 into RAX register.

### AXIOM 0.3: Memory Management Unit (MMU)

Hardware component inside CPU. Translates Virtual Addresses (VA) to Physical Addresses (PA).

### AXIOM 0.4: Page Table Entry (PTE)

8-byte value in RAM. Contains:

- Bit 0 (Present): 1 = Page exists in RAM, 0 = Page not in RAM.
- Bit 1 (Write): 1 = Writable, 0 = Read-only.
- Bit 2 (User): 1 = User accessible, 0 = Kernel only.
- Bits 12-51 (PFN): Physical Frame Number (PA >> 12).

### AXIOM 0.5: Page Fault

Exception. Occurs when MMU cannot translate VA to PA. Reasons:

1. PTE bit 0 = 0 (Page not present).
2. Access violates PTE permission bits.

---

## LEVEL 1: EXCEPTION MECHANISM

### AXIOM 1.1: Interrupt Descriptor Table (IDT)

Array in RAM. 256 entries. Each entry = 16 bytes. Contains handler addresses for exceptions/interrupts.

### AXIOM 1.2: Exception Vector

Index into IDT. Page Fault = Vector 14 (0x0E).

### AXIOM 1.3: Exception Entry Sequence (Hardware)

When exception occurs, CPU executes microcode:

1. Look up IDT[14] (Page Fault handler).
2. Push registers to stack (automatic):
   - SS (Stack Segment, 8 bytes)
   - RSP (Stack Pointer, 8 bytes)
   - RFLAGS (CPU flags, 8 bytes)
   - CS (Code Segment, 8 bytes)
   - RIP (Instruction Pointer, 8 bytes)
   - **Error Code (8 bytes, ONLY for certain exceptions)**
3. Jump to handler address from IDT[14].

### AXIOM 1.4: Error Code (Page Fault Specific)

CPU pushes Error Code AFTER RIP, BEFORE jumping to handler.

---

## LEVEL 2: ERROR CODE CONSTRUCTION

### AXIOM 2.1: Error Code Bits (x86_64 Specification)

CPU constructs Error Code using MMU state:

- Bit 0 (P): Copy of PTE bit 0 (Present flag). If MMU found PTE with P=0, Error Code bit 0 = 0.
- Bit 1 (W/R): 0 = Read access, 1 = Write access. Determined by instruction type.
- Bit 2 (U/S): 0 = Supervisor mode (CPL=0), 1 = User mode (CPL=3). Copy of CS register bits 0-1.
- Bit 3 (RSVD): 1 if reserved bit in PTE was set (rare).
- Bit 4 (I/D): 1 if fault during instruction fetch.
- Bits 5-63: Reserved (0).

### AXIOM 2.2: Your Case Derivation

**Instruction**: `strcpy` writes to 0x78d7ce727100.
**MMU State**:

- PTE for 0x78d7ce727100 does not exist (P=0).
- Instruction is `mov [0x78d7ce727100], byte` (WRITE operation).
- CS register = 0x33 (User Mode, CPL=3).

**CPU Calculates**:

- Bit 0 = 0 (PTE not present).
- Bit 1 = 1 (Write access).
- Bit 2 = 1 (User mode).
- Bit 3 = 0 (No reserved violation).
- Bit 4 = 0 (Not instruction fetch).

**Binary**: 00000110
**Hex**: 0x6
**Decimal**: 6

**Result**: CPU pushes value `6` to stack.

---

## LEVEL 3: FLAGS VARIABLE

### AXIOM 3.1: Software Variable

`flags` does not exist in hardware. Created by C compiler when `do_user_addr_fault` function is called.

### AXIOM 3.2: Stack Allocation

Line 1246: `unsigned int flags = FAULT_FLAG_DEFAULT;`

- Compiler allocates 4 bytes on kernel stack.
- Initializes with value from `FAULT_FLAG_DEFAULT` constant.

### AXIOM 3.3: FAULT_FLAG_DEFAULT

Defined in `include/linux/mm.h`:

```c
#define FAULT_FLAG_DEFAULT 0
```

Value = 0.

### AXIOM 3.4: Initial State

At `do_user_addr_fault` entry:

- Stack contains: `flags` variable = 0 (4 bytes).
- Error Code is on stack (pushed by hardware) = 6 (8 bytes, different location).

---

## FINAL DERIVATION

**Question**: Who populates Error Code and Flags?

**Answer**:

- **Error Code**: CPU hardware (microcode executing during exception entry). Value = 6. Time = t_0 (before any software runs).
- **Flags**: C compiler (stack allocation). Value = 0. Time = t_1 (after do_user_addr_fault entry).

**Proof**: Error Code exists BEFORE kernel code runs. Flags is created BY kernel code.
