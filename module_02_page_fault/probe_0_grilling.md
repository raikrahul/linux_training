
# GRILLING PROBE 0: THE HARDWARE-TO-C BRIDGE (exc_page_fault)

## 1. THE ORIGIN (Hardware Axioms)

* **Trigger**: CPU executes `STRCPY` → Touches `0x71...b100` → PTE Present bit = 0.
* **Hardware Action**:
    1. Writes `0x71...b100` to `CR2` (Control Register 2).
    2. Pushes `Error Code` to Stack.
    3. Pushes `RIP`, `CS`, `EFLAGS`, `RSP`, `SS` to Stack (for return).
    4. Jumps to IDT (Interrupt Descriptor Table) entry 14.

## 2. THE ASSEMBLY HANDOFF (`asm_exc_page_fault`)

* **Action**:
    1. Saves remaining registers (RAX, RBX, etc.) to form `struct pt_regs`.
    2. Sets Arg 1 (`RDI`) = Pointer to that stack frame (`regs`).
    3. Sets Arg 2 (`RSI`) = The hardware-pushed `Error Code`.
    4. Calls `exc_page_fault(regs, error_code)`.

## 3. GRILLING THE TASK (Counter-Questions)

### Q1: Is the Address in a Register at Entry?

* **Problem**: `exc_page_fault` signature only has `regs` and `error_code`.
* **Search**: Line 1481: `unsigned long address = read_cr2();`
* **Grill**: If the software kprobe fires **at the first instruction** of `exc_page_fault`, has `CR2` been modified?
* **Axiom**: `CR2` is only overwritten by a *new* Page Fault. Since we are inside the handler and interrupts are disabled/handled, `CR2` is stable.
* **Proactive Action**: We must call `native_read_cr2()` inside our Kprobe `pre_handler` to see what the C function is *about* to read.

### Q2: What is the "Error Code" Math?

* **Axiom (Intel Vol 3A, 4.7)**:
  * Bit 0 (P): 0 = Not Present.
  * Bit 1 (W/R): 1 = Write.
  * Bit 2 (U/S): 1 = User mode.
* **Calculation**: `(User=4) | (Write=2) | (Present=0)` = `6`.
* **Counter-question**: Why is our captured flag `0x1255` but the hardware error code is `0x6`?
* **Trace**: `exc_page_fault` takes `0x6` and converts it to `0x1255` (software flags) via logic in `do_user_addr_fault`.
* **Proof Task**: We must log `RSI` in Probe 0 and verify it equals `6`.

### Q3: How to trace it "First"?

* **Constraint**: User wants to see the address *before* the area is looked up.
* **Logic**: `exc_page_fault` is the earliest point where "Address" exists as a variable.
* **Sequence change**: We will load/unload Probe 0 **BEFORE** Probe 1.

## 4. NUMERICAL DATA STRUCTURE (Axiomatic Map)

```text
[HARDWARE] CR2 = 0x797cd3892100
[STACK]    Error Code = 0x6
[CALL]     exc_page_fault(rdi=StackPtr, rsi=0x6)
[PROBE 0]  pre_handler:
           - read_cr2() -> 0x797cd3892100
           - regs->si  -> 0x6
```

## 5. OPEN QUESTIONS

1. **Safety**: `native_read_cr2()` is a raw assembly instruction (`MOV %CR2, %RAX`). Is it safe in kprobe context?
    * *Verification needed*: Kernel uses it in `exc_page_fault` entry. It is safe.
2. **Double Fault**: What if our `pr_info` causes a page fault?
    * *Axiom*: Kprobes have "recursion" protection. It will ignore our own fault.

## 6. STATUS: PLANNING PROBE 0 COMPLETE

No code written. Grilling finished.
Ready to update the `kprobe_plan.md` chronological sequence.
