
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>

/*
 * TASK: PROBE 1 - THE API ENTRY POINT
 * -----------------------------------
 * Target: handle_mm_fault
 * Filtering: (PID == target_pid) && (Address == target_addr)
 *
 * AXIOM 0: SYSTEM V AMD64 ABI REGISTER MAPPING
 * Arg 1 (vma)     -> %rdi
 * Arg 2 (address) -> %rsi
 * Arg 3 (flags)   -> %rdx
 * Arg 4 (regs)    -> %rcx
 *
 * AXIOM 1: TARGET DATA
 * Expected Address: [Determined from userspace VA+0x100]
 * Expected Flags:   0x1255 (WRITE | USER | KILLABLE | VMA_LOCK)
 */

static int target_pid = 0;
module_param(target_pid, int, 0644);

static unsigned long target_addr = 0;
module_param(target_addr, ulong, 0644);

static char symbol_name[64] = "handle_mm_fault";
module_param_string(symbol, symbol_name, sizeof(symbol_name), 0644);

static struct kprobe kp = {
    .symbol_name = symbol_name,
};

static int handler_pre(struct kprobe *p, struct pt_regs *regs) {
  unsigned long va = regs->si;
  unsigned long fl = regs->dx;

  /* FILTER: PID + Address */
  if (target_pid != 0 && current->pid != target_pid)
    return 0;

  if (target_addr != 0 && va != target_addr)
    return 0;

  pr_info("AXIOM_TRACE: Hit %s in PID %d\n", symbol_name, current->pid);
  pr_info("AXIOM_TRACE: ADDR = 0x%lx\n", va);
  pr_info("AXIOM_TRACE: FLAG = 0x%lx\n", fl);

  return 0;
}

static int __init kprobe_init(void) {
  kp.pre_handler = handler_pre;
  int ret = register_kprobe(&kp);
  if (ret < 0)
    return ret;
  pr_info("Probe 1 planted: %s\n", symbol_name);
  return 0;
}

static void __exit kprobe_exit(void) { unregister_kprobe(&kp); }

module_init(kprobe_init);
module_exit(kprobe_exit);
MODULE_LICENSE("GPL");
