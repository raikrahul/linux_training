# Module 7: Network Stack Tracing

## Overview

This module traces data copies in the Linux network stack. You will understand why traditional networking requires multiple data copies and how to prove it with kprobes.

---

## 1. The Double-Copy Problem

### Traditional Socket Data Path

```
SEND PATH (User → Network):
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  User Buffer    COPY #1      Kernel skb      COPY #2   NIC TX   │
│  ┌─────────┐   ────────►   ┌─────────────┐  ────────►  ┌─────┐  │
│  │ "DATA"  │               │ skb->data   │   (DMA)    │ Wire│  │
│  └─────────┘               │ = "DATA"    │            └─────┘  │
│  VA: 0x7ffd               └─────────────┘                     │
│                             VA: 0xffff...                       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

RECEIVE PATH (Network → User):
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  NIC RX       COPY #3     Kernel skb      COPY #4    User Buffer│
│  ┌─────┐    ────────►   ┌─────────────┐  ────────►  ┌─────────┐ │
│  │ Wire│    (DMA)       │ skb->data   │             │ "DATA"  │ │
│  └─────┘                │ = "DATA"    │             └─────────┘ │
│                         └─────────────┘               VA: 0x7ffd│
│                          VA: 0xffff...                          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. sk_buff Structure

```c
// include/linux/skbuff.h
struct sk_buff {
    struct sk_buff *next, *prev;     // Queue linkage
    
    struct sock *sk;                  // Owner socket
    struct net_device *dev;           // Network device
    
    unsigned char *head;              // Start of buffer
    unsigned char *data;              // Start of data
    unsigned char *tail;              // End of data
    unsigned char *end;               // End of buffer
    
    unsigned int len;                 // Data length
    unsigned int data_len;            // Paged data length
    
    __u16 transport_header;           // TCP/UDP header offset
    __u16 network_header;             // IP header offset
    __u16 mac_header;                 // Ethernet header offset
    
    // ... many more fields
};
```

### Buffer Layout

```
┌───────────────────────────────────────────────────────────────┐
│                        sk_buff buffer                          │
│                                                                │
│ head ─────►┌──────────────────────────────────────────────────┐│
│            │ headroom (reserved space)                        ││
│ data ─────►├──────────────────────────────────────────────────┤│
│            │ Ethernet header   (14 bytes)                     ││
│            ├──────────────────────────────────────────────────┤│
│            │ IP header         (20 bytes)                     ││
│            ├──────────────────────────────────────────────────┤│
│            │ UDP/TCP header    (8/20 bytes)                   ││
│            ├──────────────────────────────────────────────────┤│
│            │ Payload data      (your message)                 ││
│ tail ─────►├──────────────────────────────────────────────────┤│
│            │ tailroom (reserved space)                        ││
│ end  ─────►└──────────────────────────────────────────────────┘│
│                                                                │
└───────────────────────────────────────────────────────────────┘
```

---

## 3. Copy Functions

### COPY #1: User → Kernel (Send Path)

```c
// net/core/iov_iter.c
size_t _copy_from_iter(void *addr, size_t bytes, struct iov_iter *i)
{
    // addr = kernel destination (skb->data)
    // bytes = data length
    // i = iterator over user buffer
    
    if (iter_is_ubuf(i))
        copy_from_user(addr, i->ubuf, bytes);
    else
        copy_from_iter_full(addr, bytes, i);
    
    return bytes;
}
```

### COPY #4: Kernel → User (Receive Path)

```c
// net/core/iov_iter.c
size_t _copy_to_iter(const void *addr, size_t bytes, struct iov_iter *i)
{
    // addr = kernel source (skb->data)
    // bytes = data length
    // i = iterator over user buffer
    
    if (iter_is_ubuf(i))
        copy_to_user(i->ubuf, addr, bytes);
    else
        copy_to_iter_full(addr, bytes, i);
    
    return bytes;
}
```

---

## 4. Tracing with Kprobes

### Send Path Kprobe

```c
// send_trace.c
#include <linux/module.h>
#include <linux/kprobes.h>

static struct kprobe kp_send = {
    .symbol_name = "_copy_from_iter",
};

// _copy_from_iter(void *addr, size_t bytes, struct iov_iter *i)
static int handler_send(struct kprobe *p, struct pt_regs *regs)
{
    void *dest = (void *)regs->di;       // Kernel buffer
    size_t len = regs->si;               // Byte count
    // struct iov_iter *iter = regs->dx; // User buffer info
    
    if (strcmp(current->comm, "sender") == 0) {
        pr_info("[COPY1] PID=%d dest=%px len=%zu\n",
                current->pid, dest, len);
    }
    
    return 0;
}

static int __init send_trace_init(void)
{
    kp_send.pre_handler = handler_send;
    return register_kprobe(&kp_send);
}

static void __exit send_trace_exit(void)
{
    unregister_kprobe(&kp_send);
}

module_init(send_trace_init);
module_exit(send_trace_exit);
MODULE_LICENSE("GPL");
```

### Receive Path Kprobe

```c
// recv_trace.c
#include <linux/module.h>
#include <linux/kprobes.h>

static struct kprobe kp_recv = {
    .symbol_name = "_copy_to_iter",
};

// _copy_to_iter(const void *addr, size_t bytes, struct iov_iter *i)
static int handler_recv(struct kprobe *p, struct pt_regs *regs)
{
    void *src = (void *)regs->di;        // Kernel buffer
    size_t len = regs->si;               // Byte count
    
    if (strcmp(current->comm, "receiver") == 0) {
        pr_info("[COPY4] PID=%d src=%px len=%zu\n",
                current->pid, src, len);
    }
    
    return 0;
}

static int __init recv_trace_init(void)
{
    kp_recv.pre_handler = handler_recv;
    return register_kprobe(&kp_recv);
}

static void __exit recv_trace_exit(void)
{
    unregister_kprobe(&kp_recv);
}

module_init(recv_trace_init);
module_exit(recv_trace_exit);
MODULE_LICENSE("GPL");
```

---

## 5. User Programs

### Sender

```c
// sender.c
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(9999),
        .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };
    
    char msg[] = "HELLO_NETWORK";
    printf("Sending from buffer at %p\n", msg);
    
    sendto(fd, msg, strlen(msg), 0,
           (struct sockaddr *)&addr, sizeof(addr));
    
    close(fd);
    return 0;
}
```

### Receiver

```c
// receiver.c
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(9999),
        .sin_addr.s_addr = INADDR_ANY,
    };
    
    bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    
    char buf[64] = {0};
    printf("Receiving into buffer at %p\n", buf);
    
    int n = recv(fd, buf, sizeof(buf), 0);
    printf("Received %d bytes: %s\n", n, buf);
    
    close(fd);
    return 0;
}
```

---

## 6. Complete Test

```bash
# Terminal 1: Build and load modules
$ make
$ sudo insmod send_trace.ko
$ sudo insmod recv_trace.ko

# Terminal 2: Start receiver
$ ./receiver
Receiving into buffer at 0x7fff12345678

# Terminal 3: Send data
$ ./sender
Sending from buffer at 0x7fff87654321

# Back to receiver (Terminal 2):
Received 13 bytes: HELLO_NETWORK

# Check dmesg
$ sudo dmesg | tail
[COPY1] PID=1234 dest=ffff888123456000 len=13
[COPY4] PID=1235 src=ffff888123456000 len=13
```

### Proof Summary

```
Sender user buffer:   0x7fff87654321
Kernel skb:           0xffff888123456000
Receiver user buffer: 0x7fff12345678

SAME 13 bytes "HELLO_NETWORK" copied:
  1. User → Kernel (COPY #1)
  2. Kernel → User (COPY #4)

Total: 2 CPU copies, 3 memory locations, 39 bytes used for 13 bytes of data
```

---

## 7. Practice Exercises

### Exercise 1: Measure Copy Overhead

Modify the kprobe to time the copy operations using ktime_get_ns().

### Exercise 2: Large Transfer Analysis

Send 1MB of data and compare:
- Total bytes copied
- Time spent in copy functions
- Impact on CPU utilization

### Exercise 3: Compare with RDMA

After completing Module 8, compare the copy count between socket and RDMA paths.

---

## Next Module

[Module 8: RDMA Fundamentals →](../module_08_rdma/)

[← Back to Course Index](../index.md)

---

## AXIOMATIC EXERCISES — BRUTE FORCE CALCULATION

### EXERCISE A: SK_BUFF SIZE CALCULATION

```
GIVEN:
  Ethernet header = 14 bytes
  IP header = 20 bytes
  UDP header = 8 bytes
  Payload = 1000 bytes
  headroom = 64 bytes
  tailroom = 32 bytes

TASK:

1. Total data = ___ + ___ + ___ + ___ = ___ bytes
2. Total buffer = headroom + data + tailroom = ___ + ___ + ___ = ___ bytes
3. skb->len = ___ (data only, no head/tail room)
4. skb->data - skb->head = ___ (headroom)
5. skb->end - skb->tail = ___ (tailroom)
```

### EXERCISE B: COPY ADDRESS CORRELATION

```
GIVEN kprobe output:
  [COPY1] sender PID=1234 dest=0xFFFF888112340050 len=13
  [COPY4] receiver PID=1235 src=0xFFFF888112340050 len=13

GIVEN userspace output:
  sender: buffer at 0x7FFD12345000
  receiver: buffer at 0x7FFE98765000

TASK:

1. COPY1: User VA ___ → Kernel VA ___
2. COPY4: Kernel VA ___ → User VA ___
3. Same kernel address? 0x___ = 0x___ → YES/NO
4. Same user address? 0x___ = 0x___ → YES/NO
5. Conclusion: ___ copies occurred
```

### EXERCISE C: COPY FUNCTION ARGUMENTS

```
_copy_from_iter(void *addr, size_t bytes, struct iov_iter *i)
_copy_to_iter(const void *addr, size_t bytes, struct iov_iter *i)

x86_64: arg1=RDI, arg2=RSI, arg3=RDX

TASK: Extract from regs

For _copy_from_iter:
  1. dest (kernel buffer) = regs->___ = ___
  2. len = regs->___ = ___
  3. iter = regs->___ = ___

For _copy_to_iter:
  1. src (kernel buffer) = regs->___ = ___
  2. len = regs->___ = ___
  3. iter = regs->___ = ___

GIVEN: regs->di=0xFFFF888112340000, regs->si=0x100, regs->dx=0xFFFF888198760000
  kernel buffer = 0x___
  length = ___ bytes = ___ decimal
```

### EXERCISE D: BANDWIDTH OVERHEAD

```
GIVEN:
  Transfer 1GB of data
  Each copy: CPU reads and writes each byte
  Memory bandwidth: 50 GB/s

TASK:

1. Bytes copied in send path = 1GB (user→kernel)
2. Bytes copied in recv path = 1GB (kernel→user)
3. Total bytes moved by CPU = ___ + ___ = ___ GB
4. Each copy = read + write = 2 × data size
5. Total memory operations = ___ × 2 = ___ GB
6. Time for copies = ___ GB / 50 GB/s = ___ seconds
7. With RDMA (zero copy): ___ copies = ___ seconds overhead
```

---

## FAILURE PREDICTIONS

```
FAILURE 1: skb->data is packet start, not buffer start → headroom calculation wrong
FAILURE 2: Same kernel address for send/recv → loopback shares skb? Check carefully
FAILURE 3: iter contains userspace info but is kernel struct → don't deref user pointers
FAILURE 4: len in hex 0x100 = 256 decimal, not 100
FAILURE 5: Forgetting each copy is read+write → 2x memory bandwidth
```

---

## W-QUESTIONS — NUMERICAL ANSWERS

### WHAT: sk_buff Data Layout
```
skb->head = 0xFFFF888112340000
skb->data = 0xFFFF888112340040 (64 byte headroom)
skb->tail = 0xFFFF888112340440 (1024 bytes data)
skb->end = 0xFFFF888112340500 (192 byte tailroom)
skb->len = 1024 bytes
Total buffer = 0x500 bytes = 1280 bytes
```

### WHY: Headroom Exists
```
Packet arrives with Ethernet+IP+UDP headers
Headroom allows prepending:
  - Add new header: skb_push(skb, 20) → data -= 20
  - Without headroom: allocate new buffer, copy all → slow
64 byte headroom = space for ~4 additional headers
```

### WHERE: Copy Functions
```
_copy_from_iter at 0xFFFFFFFF815A1234
_copy_to_iter at 0xFFFFFFFF815A1500
Net stack calls these, not raw copy_from_user
iter describes scatter-gather list of user buffers
```

### WHO: Initiates Copy
```
sendto() syscall → sock_sendmsg → udp_sendmsg
  → _copy_from_iter (user buf → skb)
recvfrom() syscall → sock_recvmsg → udp_recvmsg
  → _copy_to_iter (skb → user buf)
PID 1234 calls sendto → kernel copies on behalf of 1234
```

### WHEN: Each Copy Happens
```
T₁: User calls send(fd, buf, 1000)
T₂: Kernel allocates skb, COPY #1 from buf to skb->data
T₃: NIC DMA reads skb->data to wire (no CPU)
T₄: Remote NIC DMA writes to skb->data (no CPU)
T₅: User calls recv(fd, buf, 1000)
T₆: Kernel COPY #4 from skb->data to user buf
```

### WITHOUT: Zero-Copy
```
1GB transfer with copies:
  COPY #1: 1GB read + 1GB write = 2GB memory ops
  COPY #4: 1GB read + 1GB write = 2GB memory ops
  Total: 4GB memory operations
  At 50GB/s: 4GB / 50GB/s = 80ms overhead

With RDMA (zero-copy):
  DMA: 1GB NIC→RAM (no CPU)
  Total: 0 CPU copy overhead
```

### WHICH: Function for Direction
```
Send path: _copy_from_iter (user → kernel)
  regs->di = kernel dest, regs->si = length
Recv path: _copy_to_iter (kernel → user)  
  regs->di = kernel src, regs->si = length
Check comm: "sender" for COPY#1, "receiver" for COPY#4
```

---

## ANNOYING CALCULATIONS — BREAKDOWN

### Annoying: Packet Size with Headers
```
User payload = 1000 bytes
UDP header = 8 bytes
IP header = 20 bytes
Ethernet header = 14 bytes
Total on wire = 1000 + 8 + 20 + 14 = 1042 bytes
With CRC (4 bytes) = 1046 bytes
```

### Annoying: skb Pointer Math
```
skb->data = 0xFFFF888112340040
Network header at data + 14 (after Ethernet)
IP header addr = 0xFFFF888112340040 + 0xE = 0xFFFF88811234004E
UDP header at IP + 20 = 0xFFFF88811234004E + 0x14 = 0xFFFF888112340062
Payload at UDP + 8 = 0xFFFF888112340062 + 8 = 0xFFFF88811234006A
```

### Annoying: Bandwidth vs CPU
```
100 Gbps = 12.5 GB/s wire speed
Copy overhead at 50% CPU: only 6.25 GB/s effective
100 Gbps NIC, but CPU bottleneck = 50 Gbps actual
RDMA NIC: 100% wire speed, 0% CPU for data path
```

---

## ATTACK PLAN

```
1. Probe _copy_from_iter for send path
2. Probe _copy_to_iter for recv path
3. Filter by current->comm
4. Extract: regs->di (buffer), regs->si (len)
5. Correlate kernel addresses between send/recv
```

---

## ADDITIONAL FAILURE PREDICTIONS

```
FAILURE 7: skb->len is payload, not full buffer size
FAILURE 8: iter is kernel struct, cannot deref user parts directly
FAILURE 9: 0x100 = 256 decimal, not 100 → hex confusion
FAILURE 10: Loopback may reuse skb → same address for send/recv
```

---

## SHELL COMMANDS — PARADOXICAL THINKING EXERCISES

### COMMAND 1: Trace Network Copies Live

```bash
# Use bpftrace to trace _copy_to_iter
sudo bpftrace -e '
kprobe:_copy_to_iter {
    @bytes[comm] = sum(arg1);
}
interval:s:5 { exit(); }
'

# WHAT: Copy function called for every recv()
# WHY: Kernel must copy from skb to userspace
# WHERE: net/core/datagram.c, called from udp_recvmsg
# WHO: Process making recv() syscall
# WHEN: Every received packet to userspace
# WITHOUT: Zero-copy (RDMA, io_uring with registered buffers)
# WHICH: arg0=src, arg1=len, arg2=iter

# CALCULATION:
# UDP packet: 1472 bytes payload (MTU 1500 - 28 headers)
# 1000 packets/sec = 1472 × 1000 = 1.47 MB/sec copied
# CPU @ 10GB/s memcpy = 1.47MB / 10GB = 147μs/sec for copies
# NOT including kernel processing, just raw copy
```

### COMMAND 2: Compare Socket vs Raw Copies

```bash
# Send 1GB over loopback, measure copies
dd if=/dev/urandom bs=1M count=100 of=/tmp/testdata 2>/dev/null
nc -l 9999 > /dev/null &
time nc localhost 9999 < /tmp/testdata

# CALCULATION:
# 100MB file sent over socket:
# COPY #1: user buffer → kernel skb = 100MB
# COPY #4: kernel skb → receiver buffer = 100MB
# Total: 200MB of data moved by CPU
#
# At 10GB/s: 200MB / 10GB = 20ms just for copies
# Observed time ≈ 50ms → 40% of time is copying!
#
# SCALE:
# 1GB transfer: 2GB copies @ 10GB/s = 200ms
# 10GB transfer: 20GB copies @ 10GB/s = 2 seconds
# 100Gbps wire, but CPU limits to ~40Gbps effective
```

### COMMAND 3: Observe sk_buff Allocation

```bash
cat /proc/slabinfo | grep skbuff
# skbuff_head_cache, skbuff_fclone_cache

# MEMORY DIAGRAM:
# ┌─────────────────────────────────────────────────────────────────┐
# │ sk_buff structure @ 0xFFFF888112340000                          │
# │                                                                 │
# │ offset 0x00: next, prev (linked list)         16 bytes          │
# │ offset 0x10: sk (socket pointer)               8 bytes          │
# │ offset 0x18: dev (net_device)                  8 bytes          │
# │ offset 0x20: head (buffer start)               8 bytes          │
# │ offset 0x28: data (packet start)               8 bytes          │
# │ offset 0x30: tail (data end)                   4 bytes          │
# │ offset 0x34: end (buffer end)                  4 bytes          │
# │ offset 0x38: len (data length)                 4 bytes          │
# │ ...                                                             │
# │ Total: ~256 bytes per sk_buff header                            │
# │                                                                 │
# │ Separate: data buffer (default 2048 bytes)                      │
# │                                                                 │
# │ 1000 packets in flight:                                         │
# │   Headers: 1000 × 256 = 256KB                                   │
# │   Buffers: 1000 × 2048 = 2MB                                    │
# │   Total: ~2.25MB for 1000 queued packets                        │
# └─────────────────────────────────────────────────────────────────┘
```

### COMMAND 4: UDP Send Path Trace

```bash
sudo perf probe --add 'udp_sendmsg'
sudo perf probe --add '_copy_from_iter len=%si'
sudo perf record -e probe:udp_sendmsg -e probe:_copy_from_iter -- \
    dd if=/dev/zero bs=1024 count=100 | nc -u localhost 9999
sudo perf script

# TRACE shows:
# udp_sendmsg entry
# _copy_from_iter len=1024  (COPY #1)
# udp_sendmsg return
#
# CALCULATION:
# 100 sends × 1024 bytes = 102400 bytes
# _copy_from_iter called 100 times
# Each copy: 1024 bytes from user VA to kernel skb
#
# Time: ~10μs per sendmsg
# Total: 100 × 10μs = 1ms for 100 packets
```

---

## FINAL PARADOX QUESTIONS

```
Q1: Loopback is "same machine", why any copies?
    
    ANSWER:
    Sender process VA ≠ receiver process VA
    Cannot share memory (security, isolation)
    Even loopback: user→kernel→user = 2 copies
    Only way to avoid: shared memory (not sockets)
    
Q2: NIC does DMA, why is there still a copy?
    
    CALCULATION:
    NIC DMA: wire → kernel skb (no CPU, COPY #3)
    But: skb is in kernel address space
    User buffer is in user address space
    Cannot change user's page tables to point at skb
    Must copy: kernel skb → user buffer (COPY #4)
    
Q3: Zero-copy receive possible?
    
    ANSWER:
    TCP_ZEROCOPY_RECEIVE: maps skb pages into user VA
    Requires: page-aligned data, specific kernel config
    Limitation: page granularity (4KB), not byte
    RDMA: truly zero copy, NIC writes to user-registered memory
```

---

[← Previous Lesson](../module_06_kprobe_tracing/lesson_06.md) | [Course Index](../index.md) | [Next Lesson →](../module_08_rdma/lesson_08.md)
