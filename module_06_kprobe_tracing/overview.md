# Module 6: Kprobe Tracing

## Overview

This module teaches you how to instrument the running kernel using kprobes. You will write kernel modules that intercept function calls and extract runtime data.

---

## 1. What is a Kprobe?

A kprobe inserts a breakpoint instruction at any kernel address:

```
Original function:
do_page_fault:
    push rbp          ← Normal instruction
    mov rbp, rsp
    ...

With kprobe:
do_page_fault:
    int3              ← Breakpoint (0xCC)
    mov rbp, rsp
    ...
```

When CPU hits `int3`:
1. Trap to kprobe handler
2. Run your pre_handler
3. Single-step original instruction
4. Run your post_handler (optional)
5. Continue execution

---

## 2. Kprobe Structure

```c
// include/linux/kprobes.h
struct kprobe {
    // Probe location (set one of these)
    kprobe_opcode_t *addr;       // Exact address
    const char *symbol_name;     // Function name
    unsigned int offset;         // Offset into function
    
    // Handlers
    kprobe_pre_handler_t pre_handler;    // Before instruction
    kprobe_post_handler_t post_handler;  // After instruction
    
    // Internal
    struct list_head list;
    kprobe_opcode_t opcode;      // Saved original instruction
    // ...
};
```

---

## 3. Register Arguments on x86_64

When your handler runs, `regs` contains the CPU state at probe point:

```
x86_64 Calling Convention:
┌───────────┬──────────────────┬───────────────────┐
│ Argument  │ Register         │ pt_regs field     │
├───────────┼──────────────────┼───────────────────┤
│ 1st       │ RDI              │ regs->di          │
│ 2nd       │ RSI              │ regs->si          │
│ 3rd       │ RDX              │ regs->dx          │
│ 4th       │ RCX              │ regs->cx          │
│ 5th       │ R8               │ regs->r8          │
│ 6th       │ R9               │ regs->r9          │
│ Return    │ RAX              │ regs->ax          │
│ Stack Ptr │ RSP              │ regs->sp          │
│ Instr Ptr │ RIP              │ regs->ip          │
└───────────┴──────────────────┴───────────────────┘
```

---

## 4. Basic Kprobe Module

```c
// kprobe_basic.c
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/sched.h>

static struct kprobe kp = {
    .symbol_name = "do_sys_openat2",
};

// Called BEFORE the probed instruction
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    // do_sys_openat2(int dfd, const char __user *filename, ...)
    // arg1 = regs->di = dfd
    // arg2 = regs->si = filename pointer
    
    char filename[256];
    
    // Copy filename from userspace
    if (strncpy_from_user(filename, (char __user *)regs->si, 255) > 0) {
        pr_info("[OPEN] PID=%d COMM=%s FILE=%s\n",
                current->pid, current->comm, filename);
    }
    
    return 0;  // 0 = continue execution
}

static int __init kprobe_init(void)
{
    int ret;
    
    kp.pre_handler = handler_pre;
    
    ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("register_kprobe failed: %d\n", ret);
        return ret;
    }
    
    pr_info("Kprobe registered at %px\n", kp.addr);
    return 0;
}

static void __exit kprobe_exit(void)
{
    unregister_kprobe(&kp);
    pr_info("Kprobe unregistered\n");
}

module_init(kprobe_init);
module_exit(kprobe_exit);
MODULE_LICENSE("GPL");
```

### Makefile

```makefile
obj-m += kprobe_basic.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
```

### Usage

```bash
$ make
$ sudo insmod kprobe_basic.ko
$ cat /etc/passwd  # Trigger some opens
$ dmesg | tail
[OPEN] PID=1234 COMM=cat FILE=/etc/passwd
[OPEN] PID=1234 COMM=cat FILE=/lib/x86_64-linux-gnu/libc.so.6
$ sudo rmmod kprobe_basic
```

---

## 5. Kprobe for Page Faults

```c
// fault_kprobe.c
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/mm.h>

static struct kprobe kp = {
    .symbol_name = "handle_mm_fault",
};

// handle_mm_fault(struct vm_area_struct *vma, unsigned long address,
//                 unsigned int flags, struct pt_regs *regs)
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct vm_area_struct *vma = (struct vm_area_struct *)regs->di;
    unsigned long address = regs->si;
    unsigned int flags = regs->dx;
    
    // Filter by process name
    if (strcmp(current->comm, "my_program") != 0)
        return 0;
    
    pr_info("[FAULT] PID=%d ADDR=0x%lx FLAGS=0x%x "
            "VMA=[0x%lx-0x%lx] PROT=%c%c%c\n",
            current->pid,
            address,
            flags,
            vma->vm_start, vma->vm_end,
            (vma->vm_flags & VM_READ)  ? 'r' : '-',
            (vma->vm_flags & VM_WRITE) ? 'w' : '-',
            (vma->vm_flags & VM_EXEC)  ? 'x' : '-');
    
    return 0;
}

static int __init fault_kprobe_init(void)
{
    kp.pre_handler = handler_pre;
    return register_kprobe(&kp);
}

static void __exit fault_kprobe_exit(void)
{
    unregister_kprobe(&kp);
}

module_init(fault_kprobe_init);
module_exit(fault_kprobe_exit);
MODULE_LICENSE("GPL");
```

---

## 6. Kretprobe: Tracing Return Values

```c
// copy_kretprobe.c
#include <linux/module.h>
#include <linux/kprobes.h>

static struct kretprobe krp = {
    .kp.symbol_name = "_copy_from_user",
    .maxactive = 20,  // Max concurrent probes
};

// Called when function returns
static int ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    unsigned long retval = regs_return_value(regs);
    
    // _copy_from_user returns number of bytes NOT copied
    // 0 = success
    if (retval != 0) {
        pr_warn("[COPY_FAIL] PID=%d COMM=%s bytes_failed=%lu\n",
                current->pid, current->comm, retval);
    }
    
    return 0;
}

static int entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    // Can save data here to use in ret_handler
    // ri->data is available for storage
    return 0;
}

static int __init copy_kretprobe_init(void)
{
    krp.handler = ret_handler;
    krp.entry_handler = entry_handler;
    krp.data_size = 0;  // No private data
    
    return register_kretprobe(&krp);
}

static void __exit copy_kretprobe_exit(void)
{
    unregister_kretprobe(&krp);
}

module_init(copy_kretprobe_init);
module_exit(copy_kretprobe_exit);
MODULE_LICENSE("GPL");
```

---

## 7. Safety Rules

### DO NOT:

```c
// WRONG: Sleeping in handler
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    msleep(100);  // WILL CRASH - atomic context!
    kmalloc(100, GFP_KERNEL);  // WILL CRASH - can sleep!
}

// WRONG: Dereferencing without checking
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct foo *ptr = (void *)regs->di;
    pr_info("%d\n", ptr->value);  // May crash if ptr is NULL!
}
```

### DO:

```c
// CORRECT: Check pointers
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct foo *ptr = (void *)regs->di;
    
    if (!ptr)
        return 0;
    
    pr_info("%d\n", ptr->value);
}

// CORRECT: Rate limit output
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    if (printk_ratelimit())
        pr_info("...\n");
}

// CORRECT: Atomic allocations only
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    void *p = kmalloc(100, GFP_ATOMIC);  // OK
}
```

---

## 8. Practice Exercises

### Exercise 1: System Call Tracer

Create a kprobe that logs all execve() calls with the program path.

### Exercise 2: Memory Allocation Tracker

Create kretprobes on kmalloc/kfree to track allocation patterns.

### Exercise 3: Network Packet Counter

Create a kprobe on netif_receive_skb to count packets per interface.

---

## Next Module

[Module 7: Network Stack Tracing →](../module_07_network_tracing/)

[← Back to Course Index](../index.md)
