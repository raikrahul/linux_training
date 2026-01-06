
# STRICT AXIOMATIC TRACE: MALLOC PAGEFAULT (FULL MERGE + HARD PUZZLE)

## AXIOM 0: HARDWARE STATE (T=0)

**CPU**: x86_64 | **CR3** (Page Table Root): `0x10A000` (Assumed) | **Mode**: User(3)
**Instruction**: `MOV %RCX, 0x100(%RAX)` @ `RIP=0x401145`
**Registers**:
+----------------+------------------+
| Register       | Value            |
+----------------+------------------+
| RAX (Base)     | 0x7e95046af000   |
| RCX (Src)      | 0x00000000000001 |
| CR2 (Fault)    | 0x7e95046af100   | <--- WRITTEN BY CPU EXCEPTION PROTOCOL
+----------------+------------------+

## STEP 1: IDT VECTOR 14 (HARDWARE -> KERNEL STACK)

**Action**: CPU pushes context to Kernel Stack (`RSP` switches to `TSS.SP0`).
**Kernel Stack Layout**:
+----------------+------------------+------------------------------+
| Memory Offset  | Value            | Description                  |
+----------------+------------------+------------------------------+
| RSP + 40       | 0x000000000018   | SS (User Data Segment)       |
| RSP + 32       | 0x7ffd......00   | RSP (User Stack Pointer)     |
| RSP + 24       | 0x000000000246   | RFLAGS (Interrupts Enabled)  |
| RSP + 16       | 0x000000000033   | CS (User Code Segment)       |
| RSP + 8        | 0x00000000401145 | RIP (Instruction Pointer)    |
| RSP + 0        | 0x000000000015   | ErrCode (User|Write|Prot)    |
+----------------+------------------+------------------------------+

## STEP 2: ENTRY STUB (`arch/x86/entry/entry_64.S`)

**Function**: `asm_exc_page_fault`
**Action**: `PUSH` remaining regs -> `struct pt_regs`.
**Calculated `pt_regs` Address**: `0xfffffe8000...` (Kernel Stack Top).
**Arg 1 (RDI)**: `struct pt_regs *regs` = `RSP`.
**Arg 2 (RSI)**: `error_code` = `0x15` (Popped from Stack).

## STEP 3: FAULT ROUTING (`arch/x86/mm/fault.c`)

**Function**: `exc_page_fault` -> `handle_page_fault` -> `do_user_addr_fault`
**Input**: `CR2` = `0x7e95046af100`.
**Logic**:

  1. `kmmio_fault(0x7e95046af100)` -> ✗ (User Addr).
  2. `fault_in_kernel_space(...)` -> ✗.
  3. `do_user_addr_fault(...)` -> ✓.

## STEP 4: VMA LOOKUP (`mm/memory.c`)

**Data Structure**: `struct vm_area_struct` (Found via Maple Tree).
+----------------+------------------+
| Field          | Value            |
+----------------+------------------+
| vm_start       | 0x7e95046af000   |
| vm_end         | 0x7e95046b3000   | (4 Pages)
| vm_flags       | 0x000000000002   | (VM_WRITE | VM_READ)
| vm_ops         | NULL             | (Anonymous Mapping)
+----------------+------------------+
**Validation**:

  1. `0x7e95046af100` >= `vm_start`? ✓.
  2. `0x7e95046af100` <  `vm_end`?   ✓.
  3. `vm_flags & VM_WRITE`?          ✓.

## STEP 5: PAGE TABLE WALK (SOFTWARE SIMULATION)

**Input Address**: `0x7e95046af100`
**Binary**: `0000000000000000 011111101 001010100 000010000 010101111 000100000000`
**Indices**:
+-------+-----------+---------+----------------------------+
| Level | Bits      | Index   | Calculation                |
+-------+-----------+---------+----------------------------+
| PGD   | 47-39     | 253     | (Addr >> 39) & 0x1FF       |
| PUD   | 38-30     | 84      | (Addr >> 30) & 0x1FF       |
| PMD   | 29-21     | 35      | (Addr >> 21) & 0x1FF       |
| PTE   | 20-12     | 175     | (Addr >> 12) & 0x1FF       |
| Offset| 11-0      | 0x100   | (Addr & 0xFFF)             |
+-------+-----------+---------+----------------------------+
**Calculated PMD Page PFN**: Let's call it `PFN_PMD`.

## STEP 6: ALLOCATION (Buddy System)

**Function**: `alloc_anon_folio` -> `alloc_pages`.
**Request**: Order 0 (4KB). Flags: `GFP_HIGHUSER_MOVABLE | __GFP_ZERO`.
**Output**: `struct page *` (Physical RAM Object).
+----------------+------------------+
| struct page    | Value            |
+----------------+------------------+
| flags          | 0x...00000000    |
| _refcount      | 1                |
| PFN (Index)    | 0x2CAD54         | <--- THE CRITICAL NUMBER
+----------------+------------------+
**Physical Address**: `0x2CAD54` * `0x1000` = `0x2CAD54000`.

## STEP 7: PTE CONSTRUCTION (Bitwise Assembly)

**Formula**: `(PFN << 12) | PG_present | PG_rw | PG_user`.
**Calculation**:

- PFN Part: `0x2CAD54` << 12 = `0x2CAD54000`.
- Flags: `0x1` (P) | `0x2` (RW) | `0x4` (U) = `0x7`.
- Additional: `_PAGE_ACCESSED` (0x20) | `_PAGE_DIRTY` (0x40).  
- Result: `0x2CAD54000` | `0x67` = `0x2CAD54067`.

## STEP 8 (THE HARD PUZZLE): SPLIT LOCK ADDRESS DERIVATION

**Question**: Where EXACTLY is the Spinlock that protects this PTE?
**Axiom 1**: `CONFIG_SPLIT_PTLOCK_CPUS="4"` (Default).
**Axiom 2**: `CONFIG_NR_CPUS="8192"`.
**Fact**: `8192 > 4` → `ALLOC_SPLIT_PTLOCKS` is **TRUE**.
**Consequence**: The lock is **NOT** in `mm_struct`. It is attached to the **PMD Page**.

**Data Structure**: `struct ptdesc` (Overlays `struct page`).
+----------------+------+-------------------------+
| Field          | Size | Offset (Bytes)          |
+----------------+------+-------------------------+
| __page_flags   | 8    | 0                       |
| pt_list (union)| 16   | 8                       |
|__page_mapping | 8    | 24 (0x18)               |
| pt_mm (union)  | 8    | 32 (0x20)               |
| ptl (pointer!) | 8    | 40 (0x28)               | <--- HERE
+----------------+------+-------------------------+

**Calculation**:

1. **PMD PFN**: `PFN_PMD` (Value stored in PUD[84]).
2. **Struct Page Addr**: `vmemmap + (PFN_PMD * 64)`.
3. **Lock Pointer Addr**: `Struct_Page_Addr + 40`.
4. **Action**: Read 8 bytes at `Lock_Pointer_Addr`.
5. **Result**: `Heap_Address` (e.g., `0xffff8881a0b0c0d0`).
6. **The Lock**: The `spinlock_t` resides at `Heap_Address` (Allocated via Slab).

## DERIVATION INTERLUDE: ATOMIC MEMORY ACCESS

**Axiom A1 (Compiler Optimization)**: Compilers rearrange instructions to be faster. They assume variables don't change unless *they* change them.
**Axiom A2 (The Hardware)**: Hardware devices (like MMU) read RAM asynchronously. They ignore Compiler assumptions.
**Definition D1 (`volatile`)**: A request to the Compiler: "Do NOT optimize this. Read/Write exactly here, exactly now."
**Definition D2 (`typeof`)**: A generic way to ask "What type is this variable?" (e.g., `pte_t`).
**Derivation of `WRITE_ONCE(x, val)`**:

  1. We want to write `val` to address `&x`.
  2. We cast `&x` to `volatile typeof(x) *`.
     - *Meaning*: "Refer to this address as a Volatile Pointer to T".
  3. We dereference it: `*(...)`.
  4. We assign `val`.
  5. **Result**: Compiler emits a single `MOV` instruction. It cannot defer it. It cannot split it (if T is 64-bit aligned).

## STEP 9: HARDWARE LINK (`include/linux/pgtable.h`)

**Function**: `set_pte_at` -> `native_set_pte`.
**File**: `arch/x86/include/asm/pgtable_64.h:65`.
**Macro Expansion**:

  1. `ptep` is a pointer to the PTE in RAM (Virtual Address).
  2. `pte` is the 64-bit value `0x2CAD54067`.
  3. `WRITE_ONCE(*ptep, pte)` expands to:

     ```c
     *(volatile pte_t *)ptep = pte;
     ```

  4. **Instruction Generated**: `MOVQ %rsi, (%rdi)` (Move Quad-Word).
  5. **Axiom (x86_64 Atomicity)**: A 64-bit aligned write takes exactly 1 CPU cycle to commit to Cache.
  6. **Conclusion**: The MMU sees either Old Value (0) or New Value (0x2CAD54067). Never a mix.

**Pointer Arithmetic (The Physical Write)**:

- **Variables**:
  - `PTE_Index` = 175.
  - `Phys_Base_of_Page_Table` = Value in `PMD[35]`.
- **Calculation**:
  - `Phys_Addr` = `Phys_Base_of_Page_Table` + (`175` * 8 bytes).
  - `Virt_Addr (ptep)` = `Phys_Addr` + `PAGE_OFFSET` (Direct Map).
- **Action**:
  - `MOVQ 0x2CAD54067, (Virt_Addr)`.

## STEP 10: REPLAY

**Instruction**: `IRETQ` (Restores Context).
**CPU State**: `RIP` = `0x401145`.
**Re-Execute**:

  1. `MOV %RCX, 0x100(%RAX)`.
  2. VA = `0x7e95046af100`.
  3. MMU Walk checks PTE index 175.
     - Found: `0x2CAD54067`.
     - Translate: `0x2CAD54100`.
  4. Store: Write `0x1` to RAM.
