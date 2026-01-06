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
