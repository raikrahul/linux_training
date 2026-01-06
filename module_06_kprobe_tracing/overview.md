# Module 6: Kprobe Tracing

## Overview

This module teaches you how to instrument the running kernel using kprobes. You will write kernel modules that intercept function calls and extract runtime data.

## Learning Objectives

By the end of this module, you will be able to:

1. Register kprobes on kernel functions
2. Access function arguments via pt_regs
3. Extract data from kernel structures
4. Filter probes by process or condition
5. Analyze output in dmesg

## Key Concepts

### What is a Kprobe?

A kprobe places a breakpoint at a kernel function. When hit:
1. CPU traps to kprobe handler
2. Your handler runs with access to registers
3. Original function continues

```c
static struct kprobe kp = {
    .symbol_name = "do_page_fault",
    .pre_handler = my_handler,
};

register_kprobe(&kp);
```

### Accessing Arguments

On x86_64, function arguments are in registers:

| Argument | Register | pt_regs field |
|----------|----------|---------------|
| 1st | RDI | regs->di |
| 2nd | RSI | regs->si |
| 3rd | RDX | regs->dx |
| 4th | RCX | regs->cx |
| 5th | R8 | regs->r8 |
| 6th | R9 | regs->r9 |

### Handler Example

```c
static int handler_pre(struct kprobe *p, struct pt_regs *regs) {
    struct pt_regs *fault_regs = (struct pt_regs *)regs->di;
    unsigned long address = regs->si;
    
    pr_info("Fault at address: %lx\n", address);
    return 0;
}
```

### Safety Rules

1. Never sleep in handlers (atomic context)
2. Minimize work in handlers
3. Use printk_ratelimit for frequent events
4. Check pointers before dereferencing

## Hands-On Files

| File | Description |
|------|-------------|
| `probe_0_axioms.md` | Kprobe fundamentals |
| `probe_0_logic_trace.md` | Handler tracing example |
| `code/kprobe_driver.c` | Working kprobe module |

## Prerequisites

- Kernel module development basics
- x86_64 calling convention
- Module 2: Page Fault Handling

## Next Module

[Module 7: Network Stack Tracing →](../module_07_network_tracing/)
