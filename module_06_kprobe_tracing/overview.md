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

---

## AXIOMATIC EXERCISES — BRUTE FORCE CALCULATION

### EXERCISE A: REGISTER TO ARGUMENT MAPPING

```
x86_64 Calling Convention:
  arg1=RDI, arg2=RSI, arg3=RDX, arg4=RCX, arg5=R8, arg6=R9

GIVEN: Kprobe on function
  int do_sys_openat2(int dfd, const char __user *filename, struct open_how *how, size_t usize)

TASK: Map each argument

1. dfd (int) → regs->___ = ___
2. filename (char *) → regs->___ = ___
3. how (struct *) → regs->___ = ___
4. usize (size_t) → regs->___ = ___

GIVEN: regs at probe point:
  di=0xFFFFFFFF, si=0x7FFE12345678, dx=0xFFFF888112340000, cx=0x18

EXTRACT:
  dfd = ___ (hint: -1 for AT_FDCWD)
  filename = ___ (userspace pointer)
  how = ___ (kernel pointer)
  usize = ___ (24 bytes = sizeof struct open_how)
```

### EXERCISE B: KPROBE ADDRESS RESOLUTION

```
GIVEN:
  /proc/kallsyms shows:
    ffffffff812a5678 T handle_mm_fault
    ffffffff812a5680 t __handle_mm_fault
    ffffffff812a57b0 T do_user_addr_fault

TASK:

1. kp.symbol_name = "handle_mm_fault" → resolved to 0x___
2. offset to __handle_mm_fault = 0x___ - 0x___ = ___ bytes
3. If kp.offset = 8, probe address = 0x___ + 8 = 0x___
4. Probe at function+8 skips ___ bytes of prologue
```

### EXERCISE C: REGS STRUCTURE OFFSET

```
GIVEN: struct pt_regs layout (x86_64)

offset | field
-------+---------
0x00   | r15
0x08   | r14
0x10   | r13
0x18   | r12
0x20   | bp
0x28   | bx
0x30   | r11
0x38   | r10
0x40   | r9
0x48   | r8
0x50   | ax
0x58   | cx
0x60   | dx
0x68   | si
0x70   | di
0x78   | orig_ax
0x80   | ip
0x88   | cs
0x90   | flags
0x98   | sp
0xA0   | ss

TASK: Given regs pointer = 0xFFFF888100001000

1. Address of regs->di = 0x___ + 0x70 = 0x___
2. Address of regs->si = 0x___ + 0x68 = 0x___
3. Address of regs->ip = 0x___ + 0x80 = 0x___
4. Address of regs->sp = 0x___ + 0x98 = 0x___
```

### EXERCISE D: SAFE POINTER CHECK

```
GIVEN: Handler receives regs, wants to read struct from arg1

TASK: Identify safety checks needed

1. ptr = (void *)regs->di → ptr = 0x___
2. Check: ptr != NULL → ___
3. Check: ptr is kernel address? (ptr >= 0xFFFF800000000000) → ___
4. Check: ptr is aligned to struct size? → ___
5. Only then dereference: value = ptr->field

UNSAFE CODE (identify bug):
    struct foo *p = (void *)regs->di;
    pr_info("%d\n", p->value);  // Bug: ___________________
```

---

## FAILURE PREDICTIONS

```
FAILURE 1: Wrong register for argument → reading random data
FAILURE 2: Userspace pointer in kernel → cannot dereference directly
FAILURE 3: NULL pointer dereference in handler → kernel oops
FAILURE 4: Sleeping in handler (atomic context) → deadlock
FAILURE 5: printk without ratelimit → log flood, performance drop
FAILURE 6: regs->di is arg1, regs->ax is return value → confusion
```
