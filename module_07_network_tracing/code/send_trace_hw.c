/*
 * send_trace_hw.c — Kernel module to trace network send double-copy
 *
 * OBJECTIVE: Prove that send() causes 2 data movements:
 *   COPY #1: user buffer → kernel skb->data (via copy_from_iter)
 *   COPY #2: skb->data → NIC (via device driver)
 *
 * KPROBES:
 *   1. Probe on copy_from_iter: captures user VA, kernel VA, length
 *   2. Probe on dev_queue_xmit: captures skb->data address
 *
 * AXIOM DEPENDENCIES:
 *   - I07: iov_iter = struct tracking progress through user buffers
 *   - I12: msghdr contains msg_iter (the iterator)
 *   - P05: copy_from_iter(dest, len, iter) → dest=kernel, iter=user
 *
 * BUILD: make
 * LOAD: sudo insmod send_trace_hw.ko
 * TEST: ./sender (from userspace)
 * CHECK: dmesg | tail -20
 * UNLOAD: sudo rmmod send_trace_hw
 */

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/uio.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("r");
MODULE_DESCRIPTION("Trace network send double-copy for RDMA motivation");

/* ═══════════════════════════════════════════════════════════════════════════
 * KPROBE 1: copy_from_iter
 *
 * Signature (from include/linux/uio.h line 200-206):
 *   size_t copy_from_iter(void *addr, size_t bytes, struct iov_iter *i)
 *
 * Arguments on x86_64:
 *   RDI = addr (destination = kernel buffer)
 *   RSI = bytes (length to copy)
 *   RDX = i (pointer to iov_iter = source info)
 *
 * TODO: Extract user VA from iter. How?
 *   iter->__iov[0].iov_base gives user VA (if ITER_IOVEC type)
 *   OR iter->ubuf gives user VA (if ITER_UBUF type)
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int copy_from_iter_pre(struct kprobe *p, struct pt_regs *regs) {
  void *dest_addr = (void *)regs->di; /* kernel buffer address */
  size_t len = (size_t)regs->si;      /* bytes to copy */
  struct iov_iter *iter = (struct iov_iter *)regs->dx; /* iterator */

  /*
   * TODO: Filter to only network sends, not all copy_from_iter calls
   * Idea: Check if current->comm matches "sender"?
   * Or: Check if dest_addr looks like skb->data (in kmalloc range)?
   */

  /* Only print if our sender process */
  if (strncmp(current->comm, "sender", 6) == 0) {
    pr_info("[COPY1] PID=%d comm=%s dest=%px len=%zu\n", current->pid,
            current->comm, dest_addr, len);

    /*
     * TODO: Extract user VA from iter
     * This is the "from" address that proves user buffer is being read
     */
    if (iter) {
      pr_info("        iter: type=%u count=%zu\n", iter->iter_type,
              iter->count);
    }
  }

  return 0;
}

static struct kprobe kp_copy = {
    .symbol_name = "_copy_from_iter",
    .pre_handler = copy_from_iter_pre,
};

/* ═══════════════════════════════════════════════════════════════════════════
 * KPROBE 2: dev_queue_xmit (or __dev_queue_xmit)
 *
 * Signature (from include/linux/netdevice.h):
 *   int dev_queue_xmit(struct sk_buff *skb)
 *
 * Arguments on x86_64:
 *   RDI = skb (pointer to socket buffer)
 *
 * skb->data = pointer to packet data (this is where COPY #1 wrote to)
 * skb->len = packet length
 * skb->dev->name = device name (lo, wlp3s0, etc)
 *
 * TODO: Correlate skb->data with dest_addr from COPY #1
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int dev_queue_xmit_pre(struct kprobe *p, struct pt_regs *regs) {
  struct sk_buff *skb = (struct sk_buff *)regs->di;

  /*
   * TODO: Filter to only our sender's packets
   * Problem: At this point current->comm may be different (softirq? ksoftirqd?)
   * Solution: Check skb->sk->sk_socket or track skb from COPY #1
   */

  if (skb && skb->dev) {
    /* Only print loopback for now */
    if (strcmp(skb->dev->name, "lo") == 0 && skb->len > 0 && skb->len < 100) {
      pr_info("[COPY2] dev=%s skb_data=%px skb_len=%u\n", skb->dev->name,
              skb->data, skb->len);
    }
  }

  return 0;
}

static struct kprobe kp_xmit = {
    .symbol_name = "__dev_queue_xmit",
    .pre_handler = dev_queue_xmit_pre,
};

/* ═══════════════════════════════════════════════════════════════════════════
 * MODULE INIT / EXIT
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int __init send_trace_init(void) {
  int ret;

  pr_info("send_trace: loading...\n");

  ret = register_kprobe(&kp_copy);
  if (ret < 0) {
    pr_err("send_trace: register kprobe copy_from_iter failed: %d\n", ret);
    return ret;
  }
  pr_info("send_trace: kprobe copy_from_iter registered at %px\n",
          kp_copy.addr);

  ret = register_kprobe(&kp_xmit);
  if (ret < 0) {
    pr_err("send_trace: register kprobe dev_queue_xmit failed: %d\n", ret);
    unregister_kprobe(&kp_copy);
    return ret;
  }
  pr_info("send_trace: kprobe dev_queue_xmit registered at %px\n",
          kp_xmit.addr);

  pr_info("send_trace: ready. Run ./sender and check dmesg.\n");
  return 0;
}

static void __exit send_trace_exit(void) {
  unregister_kprobe(&kp_xmit);
  unregister_kprobe(&kp_copy);
  pr_info("send_trace: unloaded.\n");
}

module_init(send_trace_init);
module_exit(send_trace_exit);
