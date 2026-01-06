:AXIOM 0: THE SUBJECT
The subject is the **Page Fault Trace** for a Private Anonymous Mapping.

:1. WHAT (THE MECHANISM)
- **Math**: Mapping M: Virtual -> Physical where |Virtual| >> |Physical|.
- **Input**: Virtual Address V_addr = 0x7000.
- **State T0**: M(V_addr) = Empty.
- **Event**: CPU attempts Write W(V_addr).
- **Reaction**: Trap to Software -> Allocation A(V_addr) = P_frame -> State T1: M(V_addr) = P_frame.
- **Output**: Retry W(V_addr) succeeds.
- **Puzzle (Distinct)**:
  -   **Scenario**: A Warehouse with 1,000,000 numbered bins.
  -   **Physically**: You own 0 bins.
  -   **Order**: Item arrives for Bin #999,999.
  -   **Action**: You buy 1 Bin, label it #999,999.
  -   **Result**: Inventory = 1 Bin. Capacity = 1,000,000.
  -   **Efficiency**: 1 / 1,000,000 cost.

:2. WHY (THE ECONOMY)
- **Problem**: RAM = 16 GB (1.6 * 10^10 bytes). Process Address Space = 128 TB (1.28 * 10^14 bytes).
- **Ratio**: (1.28 * 10^14) / (1.6 * 10^10) approx 8000.
- **Implication**: We cannot store the full domain used.
- **Solution**: Demand Paging (Lazy Evaluation).
- **Example**:
  -   **Program Request**: `malloc(1 GB)`.
  -   **OS Action**: Reserve ranges [0, 10^9]. (Cost approx 0).
  -   **User Usage**: Writes to bytes 0...4095. (One Page).
  -   **Cost**: 4096 bytes RAM + Overhead.
  -   **Savings**: (10^9 - 4096) / 10^9 approx 99.9996% saved.

:3. WHERE (THE COORDINATES)
- **Hardware**: CPU Control Register `CR2`. (Holds V_addr).
- **Entry**: `arch/x86/entry/entry_64.S` (Assembly Stub).
- **Router**: `arch/x86/mm/fault.c` (`do_user_addr_fault`).
- **Logic**: `mm/memory.c` (`handle_mm_fault`).
- **Physical**: RAM Bank 0, Row X, Col Y.
- **Example**:
  -   **CR2**: `0x75e52fc77100`.
  -   **PGD**: `0xffff888000000000 + (Index << 3)`.
  -   **PFN**: `0x38944c`.
  -   **RAMAddr**: `0x38944c000`.

:4. WHO (THE ACTORS)
- **Actor 1: The CPU (Hardware)**.
  -   Detects PTE_Present == 0.
  -   Raises Exception 14.
  -   Saves `RIP`, `RSP`, `CR2`.
- **Actor 2: The Kernel (Software)**.
  -   Reads `CR2`.
  -   Search VMA Tree (O(log N)).
  -   Allocates `struct page`.
  -   Writes PTE.
- **Puzzle (Distinct)**:
  -   **User**: Swipes Credit Card (Virtual Money).
  -   **Terminal (CPU)**: "Declined" (Fault).
  -   **Bank (Kernel)**: checks Limit (VMA) -> Transfers Funds -> Approves.
  -   **Retry**: Swipe Accepted.

:5. WHEN (THE TIMELINE)
- **T0**: Instruction `MOV [0x7000], 'A'`.
- **T0 + 1 cycle**: Exception #14 fires.
- **T0 + 1000 cycles**: Kernel enters `do_page_fault`.
- **T0 + 5000 cycles**: `pud_alloc`, `pmd_alloc` checks.
- **T0 + 10000 cycles**: `alloc_pages` (Buddy Allocator) returns PFN.
- **T0 + 11000 cycles**: `set_pte_at` updates RAM.
- **T0 + 12000 cycles**: `iret` (Return from Interrupt).
- **T0 + 12001 cycles**: CPU Retries `MOV`. Success.

:6. WITHOUT (THE NEGATIVE)
- **Without Faults**: `mmap(1 GB)` must invoke `alloc_pages(262,144 pages)` immediately.
- **Consequence 1**: Boot time increases. (Zeroing 1GB RAM takes time).
- **Consequence 2**: Memory exhaustion. (Process A takes 1GB, Process B takes 1GB... RAM full).
- **Example**:
  -   Video Game loads Level 1.
  -   **With Faults**: Loads texture only when camera looks at it.
  -   **Without**: Loads ALL textures for Level 1, 2, 3... Out of Memory.

:7. WHICH (THE SPECIFIC PATH)
- **Scenario**: Write to Private Anonymous Memory.
- **Path Selection**:
  1.  `vma_is_anonymous`? Yes.
  2.  `fault & WRITE`? Yes.
  3.  `pte_none`? Yes.
  4.  -> **Path**: `do_anonymous_page`.
- **Alternative (Not Taken)**:
  -   If `!WRITE`: `do_read_fault` (Zero Page).
  -   If `!pte_none`: `wp_page_copy` (Copy on Write).

:8. FULL TRACE EXAMPLE (CONCRETE)
01. **Input**: `ptr = 0x75e52fc77100`. Content: Empty.
02. **Action**: `*ptr = 'H'`.
03. **Fault**: `CR2 = 0x75e52fc77100`. `ErrCode = 0x6` (User | Write).
04. **Walk**: `pgd_offset` -> `p4d` -> `pud` -> `pmd`.
05. **Alloc**: `alloc_pages(Order 0)` returns `PFN 0x38944c`.
06. **Compose**:
    -   PFN: `0x38944c` << 12 = `0x38944c000`.
    -   Flags: Present(1) | RW(1) | User(1) | Dirty(1) | Acc(1) = `0x67`.
    -   Data: `0x800000038944c867` (with NX bit).
07. **Store**: Write `0x800000038944c867` to PTE Address `0xffff8881...`.
08. **Resume**: CPU executes `*ptr = 'H'`.
09. **Result**: Physical Addr `0x38944c100` now contains `'H'` (0x48).
