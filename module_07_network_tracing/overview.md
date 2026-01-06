# Module 7: Network Stack Tracing

## Overview

This module traces data copies in the Linux network stack. You will understand why traditional networking requires multiple data copies and how this impacts performance.

## Learning Objectives

By the end of this module, you will be able to:

1. Trace the send path from user buffer to kernel
2. Trace the receive path from kernel to user buffer
3. Explain sk_buff structure and purpose
4. Use kprobes to capture network copies
5. Calculate the cost of double-copy

## Key Concepts

### The Double-Copy Problem

Traditional networking copies data twice:

```
SEND PATH:
User buffer → COPY #1 → Kernel sk_buff → NIC

RECEIVE PATH:
NIC → Kernel sk_buff → COPY #4 → User buffer
```

Each copy consumes CPU cycles and memory bandwidth.

### sk_buff Structure

The sk_buff (socket buffer) holds network packet data:

```c
struct sk_buff {
    unsigned char *data;    // Packet data pointer
    unsigned int len;       // Data length
    // ... many other fields
};
```

### Copy Functions

| Function | Direction | Use |
|----------|-----------|-----|
| `_copy_from_iter` | User → Kernel | sendto(), send() |
| `_copy_to_iter` | Kernel → User | recvfrom(), recv() |

### Kprobe Tracing

```c
// Trace COPY #1 (send)
kprobe on _copy_from_iter:
  source = regs->si (user buffer)
  dest = regs->di (kernel buffer)
  len = regs->dx

// Trace COPY #4 (receive)
kprobe on _copy_to_iter:
  source = regs->di (kernel buffer)
  len = regs->si
```

## Hands-On Files

| File | Description |
|------|-------------|
| `worksheet.md` | Complete trace with real addresses |
| `axiomatic_derivation.md` | Step-by-step proof |
| `packet_receive_axioms.md` | Receive path analysis |
| `code/sender.c` | UDP sender program |
| `code/receiver.c` | UDP receiver program |
| `code/send_trace_hw.c` | Send path kprobe |
| `code/recv_trace_hw.c` | Receive path kprobe |

## Prerequisites

- Module 6: Kprobe Tracing
- Basic networking concepts
- Socket programming

## Next Module

[Module 8: RDMA Fundamentals →](../module_08_rdma/)
