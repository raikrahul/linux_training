
# AXIOMATIC TRACE: HARDWARE TO lock_vma_under_rcu

## 1. CALL STACK TRACE (Axiom: Hardware to VMA)

# 1.Hardware. VA:0x78d7ce727100. ErrorCode:6. Work:CPU Trap #14 at strcpy. Error:None. RealVA:0x78d7ce727100. RealCode:0x6. Caller:0x...1416 (strcpy), Current:CPU_MMU

# 2.Entry. Value:NULL. Data:regs. Work:asm_exc_page_fault saves pt_regs to stack. Error:None. RealValue:pt_regs*. RealData:RSI=6. Caller:Hardware, Current:idtentry.h (Macro)

# 3.Call. address:0x...0. Data:CR2. Work:exc_page_fault reads address = read_cr2(). Error:None. RealVA:0x78d7ce727100. RealData:0x78d7ce727100. Caller:idtentry.h, Current:fault.c:1481

# 4.Call. regs,code,address. Data:6,0x...100. Work:exc_page_fault calls handle_page_fault. Error:None. RealCode:6. RealVA:0x78d7ce727100. Caller:fault.c:1481, Current:fault.c:1523

# 5.Dispatch. regs,code,address. Data:6,0x...100. Work:handle_page_fault calls do_user_addr_fault. Error:None. RealVA:0x78d7ce727100. RealData:UserMode=True. Caller:fault.c:1523, Current:fault.c:1467

# 6.Logic. flags:0. Data:None. Work:do_user_addr_fault starts. Error:None. RealValue:0. RealData:0. Caller:fault.c:1467, Current:fault.c:1255

# 7.Derive. flags:0x1. Data:WRITE. Work:X86_PF_WRITE (6 & 2) -> flags |= FAULT_FLAG_WRITE. Error:None. RealValue:0x1. RealData:0x1. Caller:fault.c:1255, Current:fault.c:1290

# 8.Call. mm,address. Data:mt,0x...100. Work:do_user_addr_fault calls lock_vma_under_rcu. Error:None. RealVA:0x78d7ce727100. RealData:0x78d7ce727100. Caller:fault.c:1290, Current:fault.c:1316

## 2. PSEUDO-DEBUGGER TRACE: lock_vma_under_rcu

# 9.Call. mm, address. Data:mt, 0x78d7ce727100. Work:lock_vma_under_rcu entry. Error:None. RealVA:0x78d7ce727100. RealData:86356. Caller:fault.c:1316, Current:memory.c:5695

# 10.Work. mas, mm_mt, address. Data:0, 0x...100. Work:MA_STATE expansion. Error:None. RealVA:0x78d7ce727100. RealData:0x78d7ce727100. Caller:memory.c:5695, Current:memory.c:5698

# 11.Lock. nesting. Data:1. Work:rcu_read_lock(). Error:None. RealValue:1. RealData:1. Caller:memory.c:5698, Current:memory.c:5701

# 12.Walk. mas. Data:mt. Work:mas_walk(&mas) tree traversal. Error:None. RealValue:struct vma*. RealData:0xffff8881... Caller:memory.c:5701, Current:memory.c:5703

# 13.VMA_Lock. vma. Data:0x... Work:vma_start_read(vma) per-VMA trylock. Error:None. RealValue:TRUE. RealData:1. Caller:memory.c:5703, Current:memory.c:5707

# 14.Anon_Check. vma. Data:anon. Work:vma_is_anonymous and anon_vma check. Error:None. RealValue:FALSE. RealData:0. Caller:memory.c:5707, Current:memory.c:5716

# 15.Range. address, start, end. Data:0x... Work:Verify VA is within VMA bounds. Error:None. RealValue:VALID. RealData:1. Caller:memory.c:5716, Current:memory.c:5720

# 16.Unlock. nesting. Data:0. Work:rcu_read_unlock(). Error:None. RealValue:0. RealData:0. Caller:memory.c:5720, Current:memory.c:5731

# 17.Return. vma. Data:0x... Work:Success. Return pointer to handle_mm_fault. Error:None. RealValue:vma*. RealData:0xffff8881... Caller:memory.c:5731, Current:memory.c:5732

## 3. SEARCH AXIOM: THE FIRST FINDER

* **Axiom 1**: \`exc_page_fault\` reads \`CR2\` register → Defines the \`address\` variable bytes.
* **Axiom 2**: \`do_user_addr_fault\` dereferences \`current->mm\` → Defines the process address space boundary (\`mm\`).
* **Axiom 3**: \`lock_vma_under_rcu\` is the first function to receive both (\`mm\`, \`address\`).
* **Computation**: \`vma = mas_walk(&mas)\`
* **Logic**: No \`vma\` object exists in RAM for this fault until this line executes.
* **Proof**: \`vma\` is NULL until \`memory.c:5703\`.
∴ **lock_vma_under_rcu** is the first function where the kernel searches for and finds the VMA.

## 4. HARDWARE ORIGIN AXIOM (The Root)

* **Vector 14**: The CPU is hardwired to jump to the address in the IDT at index 14 on a Page Fault.
* **IDT Mapping**: `arch/x86/kernel/idt.c:71` maps `X86_TRAP_PF` (14) to `asm_exc_page_fault`.
* **Variable 0**: The ONLY variables that exist at `t=0` are:
    1. `CR2` register (Faulting Address).
    2. `Error Code` on stack (Hardware pushed).
* **Derivation**: Every other variable (`mm`, `vma`, `regs`) is derived by software instructions starting from `asm_exc_page_fault`.

---
TERMS INTRODUCED WITHOUT DERIVATION: None.
