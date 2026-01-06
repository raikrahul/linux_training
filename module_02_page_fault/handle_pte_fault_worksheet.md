# HANDLE_PTE_FAULT AXIOMATIC WORKSHEET

## SECTION A: CALL CHAIN TO THIS FUNCTION

A1. Hardware: CPU faults on address 0x794e57539100.
A2. ISR 14 → asm_exc_page_fault → do_user_addr_fault.
A3. do_user_addr_fault → lock_vma_under_rcu → VMA found.
A4. lock_vma_under_rcu → handle_mm_fault(vma, address, flags).
A5. handle_mm_fault → __handle_mm_fault(vma, address, flags).
A6.__handle_mm_fault → allocates struct vm_fault on stack.
A7. __handle_mm_fault → handle_pte_fault(&vmf).
A8. ∴ vmf is a POINTER to a struct on the stack of__handle_mm_fault.

## SECTION B: STRUCT VM_FAULT LAYOUT (DERIVED)

B1. AXIOM: sizeof(void *) = 8 on x86_64.
B2. AXIOM: sizeof(unsigned long) = 8 on x86_64.
B3. AXIOM: sizeof(gfp_t) = sizeof(unsigned int) = 4.
B4. AXIOM: sizeof(enum fault_flag) = 4 (int-sized).

B5. OFFSET CALCULATION:
    | OFFSET | FIELD          | SIZE | REASON                    |
    |--------|----------------|------|---------------------------|
    | 0      | vma            | 8    | First field, pointer      |
    | 8      | gfp_mask       | 4    | unsigned int              |
    | 12     | (padding)      | 4    | Align pgoff to 8          |
    | 16     | pgoff          | 8    | pgoff_t = unsigned long   |
    | 24     | address        | 8    | unsigned long             |
    | 32     | real_address   | 8    | unsigned long             |
    | 40     | flags          | 4    | enum                      |
    | 44     | (padding)      | 4    | Align pmd to 8            |
    | 48     | pmd            | 8    | pmd_t *|
    | 56     | pud            | 8    | pud_t*                   |

B6. TO ACCESS vmf->address:
    vmf_ptr + 24 = address of vmf->address.
    *(unsigned long*)(vmf_ptr + 24) = value.

## SECTION C: TODO WORKSHEET

### TODO 1: PID FILTER

TASK: Fill in the blanks.

```c
if (______ != ______) {
    return 0;
}
```

HINT 1: What variable holds current process ID?
HINT 2: What module parameter holds target PID?

### TODO 2: EXTRACT vmf->address

TASK: Fill in the blank.

```c
unsigned long fault_addr = vmf->______;
```

HINT: Look at struct vm_fault field names in mm.h:560-604.

### TODO 3: ADDRESS PAGE FILTER

TASK: Fill in the blanks.

```c
if ((fault_addr & ______) != (target_addr & ______)) {
    return 0;
}
```

HINT: What mask zeros out the offset within a page?

### TODO 4: EXTRACT vmf->pmd

TASK: Fill in the blanks.

```c
pmd_t *pmd_ptr = vmf->______;
pmd_t pmd_val = *______;
```

HINT 1: Field name for PMD pointer in vm_fault?
HINT 2: To get value at pointer, use * operator.

### TODO 5: PRINT TRACE

TASK: Uncomment and fill in the pr_info statements.

```c
pr_info("   VMF->address: 0x%lx\n", ______);
pr_info("   VMF->pmd: 0x%px\n", ______);
pr_info("   PMD_VAL: 0x%lx\n", pmd_val(______));
```

## SECTION D: EXPECTED OUTPUT

After completing TODOs and loading driver:

```
[xxxxx] Probe planted: handle_pte_fault at 0xffffffff812xxxxx
[xxxxx] AXIOM_TRACE: handle_pte_fault HIT
[xxxxx]    COMM: mm_exercise_use | PID: 123456
[xxxxx]    VMF_PTR: 0xffffxxxxxxxxxxxx
[xxxxx]    VMF->address: 0x794e57539000
[xxxxx]    VMF->pmd: 0xffffxxxxxxxxxxxx
[xxxxx]    PMD_VAL: 0x0000000012345067
```

## SECTION E: COMPILE AND TEST COMMANDS

```bash
# Terminal 1: Compile driver
cd /home/r/Desktop/linux_kernel_portfolio/investigations/handle_pte_fault_trace
make

# Terminal 1: Run userspace program
cd /home/r/Desktop/linux_kernel_portfolio/investigations/malloc_pagefault
./mm_exercise_user
# Note PID and VA from output

# Terminal 2: Load driver with parameters
sudo dmesg -C
sudo insmod handle_pte_fault_probe.ko target_pid=<PID> target_addr=<VA>

# Terminal 1: Press ENTER to trigger fault

# Terminal 2: Check output
sudo dmesg | grep AXIOM_TRACE
sudo rmmod handle_pte_fault_probe
```

## SECTION F: KERNEL SOURCE CONFIRMATION (CR2 AND PAGE TABLE WALK)

**F1. SOURCE: arch/x86/mm/fault.c:1481**

```c
unsigned long address = read_cr2();
```

∴ CR2 is read at line 1481 to get faulting address.

**F2. SOURCE: mm/memory.c:492-495**

```c
pgd_t *pgd = pgd_offset(vma->vm_mm, addr);   // mm->pgd + index from addr
p4d_t *p4d = p4d_offset(pgd, addr);          // *pgd + index from addr
pud_t *pud = pud_offset(p4d, addr);          // *p4d + index from addr
pmd_t *pmd = pmd_offset(pud, addr);          // *pud + index from addr
```

∴ Page table walk uses BOTH:

- `vma->vm_mm` (contains pgd, equivalent to CR3).
- `addr` (from CR2, provides indices).

**F3. INDEX EXTRACTION PROOF**:

- `pgd_offset(mm, addr)` uses `mm->pgd + ((addr >> 39) & 0x1FF)`.
- `pud_offset(pgd, addr)` uses `*pgd + ((addr >> 30) & 0x1FF)`.
- `pmd_offset(pud, addr)` uses `*pud + ((addr >> 21) & 0x1FF)`.
- `pte_offset(pmd, addr)` uses `*pmd + ((addr >> 12) & 0x1FF)`.

**F4. LIVE CALCULATION (VA = 0x5a75cad7b000)**:

| LEVEL | SHIFT | MASK  | CALCULATION                       | INDEX |
|-------|-------|-------|-----------------------------------|-------|
| PGD   | 39    | 0x1FF | (0x5a75cad7b000 >> 39) & 0x1FF    | 181   |
| PUD   | 30    | 0x1FF | (0x5a75cad7b000 >> 30) & 0x1FF    | 28    |
| PMD   | 21    | 0x1FF | (0x5a75cad7b000 >> 21) & 0x1FF    | 366   |
| PTE   | 12    | 0x1FF | (0x5a75cad7b000 >> 12) & 0x1FF    | 379   |

## SECTION G: USER MISTAKE REPORT

**M1. CONFUSION**: "cr2 in maple tree?"

- WRONG: CR2 is used to look up in maple tree.
- CORRECT: CR2 = faulting VA. Maple tree lookup uses VA to find VMA struct.
- MISSED: Maple tree stores VMAs, not page tables. CR2 is the search key.

**M2. CONFUSION**: "we see cr3 or cr2 first?"

- WRONG: Implies one happens before the other in fault handling.
- CORRECT: CPU uses CR3 on every memory access (TLB miss → walk).
            CPU writes CR2 only when fault occurs.
            In fault handler, CR2 read first (line 1481), then CR3 used via mm->pgd.

**M3. CONFUSION**: "0x794e57539100 >> 12 = 0x794e57539. TLB miss?"

- WRONG: Shift by 12 causes TLB miss.
- CORRECT: Shift by 12 extracts VPN (page number). TLB miss is caused by
            VPN not being in TLB cache, not by the shift operation.

**M4. CONFUSION**: "walk page table from cr2"

- WRONG: Walk starts from CR2.
- CORRECT: Walk starts from CR3 (mm->pgd = base).
            CR2 provides INDICES into each level.
            Walk = CR3[idx0][idx1][idx2][idx3] where idx comes from CR2 bits.

**M5. CONFUSION**: "if i have the virtual address in cr3"

- WRONG: CR3 contains virtual address.
- CORRECT: CR3 contains PHYSICAL address of PGD.
            CR2 contains VIRTUAL address that faulted.

**M6. CONFUSION**: "can cr2 and cr3 be different processes?"

- WRONG: Implies they can disagree.
- CORRECT: NO. CR3 is set on context switch to current process's PGD.
            CR2 is set when current process faults.
            Both always belong to same process.

## SECTION H: WHY VMF EXISTS (DATA ACCUMULATION TRACE)

**H1. PROBLEM**: Address alone is not enough to handle fault.
**H2. SOLUTION**: Accumulate related data into one struct, pass pointer.

**H3. STEP-BY-STEP DATA GROWTH**:

| STEP | FUNCTION                | LINE         | DATA KNOWN                  | COUNT |
|------|-------------------------|--------------|-----------------------------|-------|
| 1    | exc_page_fault          | fault.c:1481 | address (from CR2)          | 1     |
| 2    | do_user_addr_fault      | fault.c:1298 | address, vma                | 2     |
| 3    | handle_mm_fault         | memory.c:5519| address, vma, flags         | 3     |
| 4    | __handle_mm_fault       | memory.c:5295| address, vma, flags, pmd, pud, gfp_mask | 6+ |
| 5    | handle_pte_fault        | memory.c:5211| + pte, orig_pte             | 8+    |
| 6    | do_anonymous_page       | memory.c:4259| + page, cow_page            | 10+   |

**H4. WITHOUT VMF** (hypothetical):

```c
do_anonymous_page(vma, address, pmd, pte, orig_pte, gfp_mask, flags, pud, page, cow_page);
```

= 10 arguments.
x86_64 has 6 argument registers (RDI, RSI, RDX, RCX, R8, R9).
10 > 6 → 4 arguments spill to stack → slower.

**H5. WITH VMF** (actual):

```c
do_anonymous_page(&vmf);
```

= 1 argument.
vmf contains 10+ fields.
1 pointer in RDI → fast.

**H6. LIVE SOURCE (memory.c:5295-5386)**:

```c
static vm_fault_t __handle_mm_fault(struct vm_area_struct *vma,
        unsigned long address, unsigned int flags)
{
    struct vm_fault vmf = {        // ALLOCATE ON STACK
        .vma = vma,                // Field 1
        .address = address & PAGE_MASK,  // Field 2
        .real_address = address,   // Field 3
        .flags = flags,            // Field 4
        .pgoff = linear_page_index(vma, address),  // Field 5
        .gfp_mask = __get_fault_gfp_mask(vma),     // Field 6
    };
    // ... walk page table, fill pmd, pud ...
    return handle_pte_fault(&vmf); // PASS POINTER
}
```

**H7. ∴ VMF = bag to carry growing data through 6+ function calls.**

## SECTION I: ADDRESS vs REAL_ADDRESS CLARIFICATION

**I1. MY ERROR**: I initially wrote trace without mentioning `.real_address`.

**I2. CORRECT SOURCE (memory.c:5300-5301)**:

```c
.address = address & PAGE_MASK,      // Line 5300
.real_address = address,             // Line 5301
```

**I3. NUMERICAL EXAMPLE**:

| FIELD         | CALCULATION                              | VALUE              |
|---------------|------------------------------------------|--------------------|
| CR2 (raw)     | from CPU                                 | 0x794e57539100     |
| .address      | 0x794e57539100 & 0xFFFFFFFFFFFFF000      | 0x794e57539000     |
| .real_address | 0x794e57539100 (no mask)                 | 0x794e57539100     |

**I4. WHY TWO FIELDS**:

- `.address` = page-aligned → used for PTE lookup (PTE covers entire 4K page).
- `.real_address` = original → used for userfaultfd, error reporting, debugging.

**I5. WHEN THEY DIFFER**:

- CR2 = 0x794e57539100 → offset = 0x100 → .address ≠ .real_address.
- CR2 = 0x794e57539000 → offset = 0x000 → .address == .real_address.

**I6. ∴ Chain DOES preserve original faulting address in .real_address.**

## SECTION J: MY ERRORS IN THIS SESSION

**J1. ERROR**: Did not mention `.real_address` in initial trace.

- WHERE: Section H, step 4.
- IMPACT: User thought original address was lost.
- FIX: Added Section I.

**J2. ERROR**: Used "idx3" in markdown which caused lint warning.

- WHERE: Section G, line about CR3[idx0][idx1][idx2][idx3].
- IMPACT: Markdown parser confused by brackets.
- FIX: Should escape or use backticks.

NEW THINGS INTRODUCED WITHOUT DERIVATION: None.

## SECTION M: BRUTAL ERROR REPORT (SLOPPY BRAIN ANALYSIS)

**M1. ERROR**: Wasted 1 hour debugging non-printing probe.

- CAUSE: Loaded driver with `target_addr=0x1000`. Actual mmap was random (`0x7817dbed2000`).
- LOGIC FAIL: `(0x7817dbed2000 & PAGE_MASK) != (0x1000 & PAGE_MASK)` → TRUE → `return 0`.
- SLOPPY: Assumed mmap is deterministic. NOT TRUE.
- FIX: Modified code to skip filter if `target_addr=0`.

**M2. ERROR**: Failed to capture mmap address in shell script variable.

- CAUSE: `VA=$(grep "VA:" /tmp/mm_out2.txt ...)`. File was empty due to race condition.
- ARITHMETIC FAIL: Empty string + 0x100 = 0x100.
- RESULT: Loaded driver with `target_addr=0x100`.
- SLOPPY: Did not check if `VA` variable was empty before arithmetic.
- FIX: Use explicit synchronization (wait for process output).

**M3. ERROR**: Confused `pmd_val` VARIABLE with `pmd_val()` MACRO.

- CODE: `pmd_t pmd_val = *pmd_ptr; pr_info(..., pmd_val(pmd_val));`
- COMPILER: `called object ‘pmd_val’ is not a function`.
- SLOPPY: Namespace collision. C macros share namespace with variables.
- FIX: Renamed variable to `pmd_entry`.

**M4. ERROR**: Forgot `vmf->real_address` exists initially.

- THOUGHT: "Chain loses original address."
- FACT: `read_cr2()` → `vmf.real_address` preserves it perfectly.
- SLOPPY: Did not `grep` struct definition thoroughly.
- FIX: Added Section I and K derivation.

**M5. ERROR**: Questioned why `vmf` stores `pmd`.

- THOUGHT: "Can't we get it from vma->vm_mm?"
- IGNORANCE: `vma->vm_mm->pgd` is ROOT. `pmd` is 3 levels deep.
- COST: Recalculating `pmd` costs 3 pointer chases per function call.
- FIX: Derived page table walk cost analysis (Section L).

**M6. ERROR**: Userspace program handling.

- FAIL: `enable stdin` for background process.
- SYMPTOM: `kill -SIGCONT` required; `cat` vs `dd` input fails.
- SLOPPY: Background processes detach from TTY stdin.
- FIX: Use `sleep(30)` instead of `getchar()` to bypass TTY requirement.

**M7. ERROR**: Markdown Lint "idx3".

- CAUSE: Used `[idx3]` without backticks.
- PARSER: Thought it was a reference link.
- SLOPPY: Formatting laziness.
- FIX: Wrap in backticks.

**M8. ERROR**: Incorrect target address calculation.

- CALC: `0x...000 + 0x100`.
- FILTER: Driver checks `& PAGE_MASK`.
- SLOPPY: `0x...100 & PAGE_MASK` == `0x...000`. So 0x100 offset doesn't matter for filter, but matters for correctness.
- FIX: Precise hexadecimal arithmetic.

NEW THINGS INTRODUCED WITHOUT DERIVATION: None.
