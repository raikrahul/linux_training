/*
 * recv_trace_hw.c — Kernel module to trace COPY #4 (kernel skb → user buffer)
 *
 * YOUR MACHINE DATA:
 * - _copy_to_iter at 0xffffffffa2938730 (from /proc/kallsyms)
 * - Receiver PID was 40761
 * - Receiver buf VA was 0x7ffe943c11d0
 * - After recv(): "HELLO_SEND_TRACE" appeared in buf
 *
 * AXIOM: _copy_to_iter(void *addr, size_t bytes, struct iov_iter *i)
 *        copies FROM addr TO iter destination
 *        addr = kernel source (skb->data)
 *        i = describes user buffer destination
 *
 * CHAIN OF THOUGHT:
 * receiver calls recv(fd=3, buf=0x7ffe943c11d0, 16, 0)
 * → syscall __sys_recvfrom (syscall number = TODO_E01)
 * → sock_recvmsg()
 * → udp_recvmsg()
 * → skb_copy_datagram_msg()
 * → _copy_to_iter(skb->data + offset, len, &msg->msg_iter)
 * → CPU writes: RAM[PA(user buf)] = RAM[PA(skb->data)]
 *
 * Build: make
 * Load: sudo insmod recv_trace_hw.ko
 * Unload: sudo rmmod recv_trace_hw
 */

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/uio.h>       /* struct iov_iter */
#include <linux/interrupt.h> /* in_interrupt() */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YOU");
MODULE_DESCRIPTION("Trace COPY #4: kernel skb -> user buffer");

/* ════════════════════════════════════════════════════════════════════════════
 * KPROBE ON _copy_to_iter — CATCHES COPY #4
 *
 * FUNCTION SIGNATURE (from lib/iov_iter.c:259):
 *   size_t _copy_to_iter(const void *addr, size_t bytes, struct iov_iter *i)
 *
 * REGISTER MAPPING (x86_64 ABI):
 *   RDI = addr   (kernel source address, this is skb->data + offset)
 *   RSI = bytes  (number of bytes to copy)
 *   RDX = i      (pointer to struct iov_iter, describes user destination)
 *
 * YOUR DATA:
 *   When receiver calls recv(), this function is called with:
 *   - addr = 0xffff8cb0???????? (kernel skb data, unknown until probed)
 *   - bytes = 16 (your message length)
 *   - i->ubuf = 0x7ffe943c11d0 (receiver's buffer VA)
 * ════════════════════════════════════════════════════════════════════════════
 */
static struct kprobe kp_copy_to_iter;

/*
 * TODO_E02: FILL IN THE HANDLER
 *
 * WHAT YOU NEED TO EXTRACT:
 * 1. source = regs->di (kernel VA where data comes FROM)
 * 2. bytes = regs->si (how many bytes copied)
 * 3. iter = (struct iov_iter *)regs->dx (describes destination)
 *
 * FROM iter, YOU NEED TO GET user destination VA:
 * - If iter->iter_type == ITER_UBUF: user VA = iter->ubuf
 * - If iter->iter_type == ITER_IOVEC: user VA = iter->__iov->iov_base
 *
 * TRAP: Which iter_type does recv() use? You must CHECK.
 *       iter_type values: ITER_UBUF=1, ITER_IOVEC=0, etc.
 *
 * PRINT:
 *   [COPY4] PID=40761 comm=receiver src=0xffff... dest=0x7ffe... len=16
 */
static int handler_copy_to_iter(struct kprobe *p, struct pt_regs *regs) {
  /* 
   * recv() runs in PROCESS context (syscall from user), NOT interrupt context.
   * So we don't need in_interrupt() check. That was filtering everything out!
   */
  
  /* Extract args first - these are always safe to read from regs */
  const void *source = (const void *)regs->di;
  size_t len = (size_t)regs->si;

  /* Filter only "receiver" process */
  if (strncmp(current->comm, "receiver", 8) != 0)
    return 0;

  pr_info("[COPY4] PID=%d comm=%s src=%px len=%zu\n",
          current->pid, current->comm, source, len);

  return 0;
}


/* ════════════════════════════════════════════════════════════════════════════
 * MODULE INIT — REGISTER KPROBE
 *
 * CHAIN:
 * 1. Set kp_copy_to_iter.symbol_name = "_copy_to_iter"
 * 2. Set kp_copy_to_iter.pre_handler = handler function
 * 3. Call register_kprobe(&kp_copy_to_iter)
 * 4. If fails, return error code
 *
 * TRAP: "_copy_to_iter" starts with underscore. Different from "copy_to_iter".
 *       /proc/kallsyms shows: ffffffffa2938730 T _copy_to_iter
 * ════════════════════════════════════════════════════════════════════════════
 */
static int __init recv_trace_init(void) {
  int ret;

  kp_copy_to_iter.symbol_name = "_copy_to_iter"; /* FROM /proc/kallsyms */
  kp_copy_to_iter.pre_handler = handler_copy_to_iter;

  ret = register_kprobe(&kp_copy_to_iter);
  if (ret < 0) {
    pr_err("recv_trace: register_kprobe _copy_to_iter failed: %d\n", ret);
    return ret;
  }

  pr_info("recv_trace: loaded, probing _copy_to_iter at %px\n",
          kp_copy_to_iter.addr);

  return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * MODULE EXIT — UNREGISTER KPROBE
 * ════════════════════════════════════════════════════════════════════════════
 */
static void __exit recv_trace_exit(void) {
  unregister_kprobe(&kp_copy_to_iter);
  pr_info("recv_trace: unloaded\n");
}

module_init(recv_trace_init);
module_exit(recv_trace_exit);
