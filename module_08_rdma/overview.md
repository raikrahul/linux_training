# Module 8: RDMA Fundamentals

## Overview

This module introduces Remote Direct Memory Access (RDMA), a technology that eliminates CPU-mediated data copies in networking.

---

## 1. RDMA vs Traditional Networking

### Traditional Path (Sockets)

```
┌──────────────────────────────────────────────────────────────┐
│ Application                                                  │
│    │                                                         │
│    │ send(fd, buf, len)                                      │
│    ▼                                                         │
│ ┌─────────────────┐                                          │
│ │ Kernel (socket) │ ◄─── CPU copy #1                         │
│ │    sk_buff      │                                          │
│ └────────┬────────┘                                          │
│          │                                                   │
│          ▼                                                   │
│ ┌─────────────────┐                                          │
│ │   NIC Driver    │ ◄─── DMA to NIC                          │
│ └────────┬────────┘                                          │
│          │                                                   │
│          ▼                                                   │
│      [  Wire  ]                                              │
└──────────────────────────────────────────────────────────────┘

Latency: ~10-50 microseconds
CPU: Involved in every packet
Copies: 2 per packet (send + receive)
```

### RDMA Path

```
┌──────────────────────────────────────────────────────────────┐
│ Application                                                  │
│    │                                                         │
│    │ ibv_post_send(qp, wr, ...)                              │
│    ▼                                                         │
│ ┌─────────────────┐                                          │
│ │ User Buffer     │ ◄─── Memory registered with ibv_reg_mr   │
│ │ (pinned in RAM) │                                          │
│ └────────┬────────┘                                          │
│          │                                                   │
│          │  DMA directly from user buffer                    │
│          ▼                                                   │
│ ┌─────────────────┐                                          │
│ │   RDMA NIC      │ ◄─── No kernel involved!                 │
│ │   (RNIC)        │                                          │
│ └────────┬────────┘                                          │
│          │                                                   │
│          ▼                                                   │
│      [  Wire  ]                                              │
└──────────────────────────────────────────────────────────────┘

Latency: ~1-2 microseconds
CPU: Not involved in data path
Copies: 0 (zero-copy)
```

---

## 2. RDMA Concepts

### Memory Registration

Before RDMA can access memory, it must be registered:

```c
struct ibv_mr *mr = ibv_reg_mr(
    pd,                          // Protection domain
    buffer,                      // Virtual address
    size,                        // Buffer size
    IBV_ACCESS_LOCAL_WRITE |     // Allow local writes
    IBV_ACCESS_REMOTE_WRITE |    // Allow remote writes
    IBV_ACCESS_REMOTE_READ       // Allow remote reads
);
```

What happens:
1. Pages are pinned (no swap, no migration)
2. Physical addresses are recorded
3. NIC is given translation table
4. lkey/rkey returned for operations

### Queue Pairs (QP)

```
┌─────────────────────────────────────────────────────────────┐
│                     Queue Pair (QP)                          │
│                                                              │
│  ┌─────────────────────┐    ┌─────────────────────────────┐ │
│  │    Send Queue       │    │    Receive Queue            │ │
│  │                     │    │                             │ │
│  │ [Work Request 0]    │    │ [Work Request 0]            │ │
│  │ [Work Request 1]    │    │ [Work Request 1]            │ │
│  │ [Work Request 2]    │    │                             │ │
│  │        ...          │    │                             │ │
│  └──────────┬──────────┘    └──────────────┬──────────────┘ │
│             │                              │                 │
│             ▼                              │                 │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              Completion Queue (CQ)                    │   │
│  │                                                       │   │
│  │ [Completion 0] [Completion 1] [Completion 2] ...      │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### RDMA Operations

| Operation | Description | Remote CPU? |
|-----------|-------------|-------------|
| SEND | Push data to remote receive buffer | Wake |
| RECV | Prepare buffer for incoming SEND | - |
| WRITE | Write to remote memory | No |
| READ | Read from remote memory | No |

---

## 3. Basic RDMA Code

### Setup

```c
#include <infiniband/verbs.h>

// 1. Get device list
struct ibv_device **dev_list = ibv_get_device_list(NULL);
struct ibv_context *ctx = ibv_open_device(dev_list[0]);

// 2. Allocate protection domain
struct ibv_pd *pd = ibv_alloc_pd(ctx);

// 3. Create completion queue
struct ibv_cq *cq = ibv_create_cq(ctx, 10, NULL, NULL, 0);

// 4. Create queue pair
struct ibv_qp_init_attr qp_attr = {
    .send_cq = cq,
    .recv_cq = cq,
    .qp_type = IBV_QPT_RC,  // Reliable Connection
    .cap = {
        .max_send_wr = 10,
        .max_recv_wr = 10,
        .max_send_sge = 1,
        .max_recv_sge = 1,
    },
};
struct ibv_qp *qp = ibv_create_qp(pd, &qp_attr);

// 5. Register memory
char buffer[4096];
struct ibv_mr *mr = ibv_reg_mr(pd, buffer, sizeof(buffer),
    IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
```

### RDMA Write (Zero-Copy)

```c
// Remote side has shared: raddr (address), rkey (remote key)

struct ibv_sge sge = {
    .addr = (uintptr_t)buffer,
    .length = data_len,
    .lkey = mr->lkey,
};

struct ibv_send_wr wr = {
    .opcode = IBV_WR_RDMA_WRITE,
    .sg_list = &sge,
    .num_sge = 1,
    .send_flags = IBV_SEND_SIGNALED,
    .wr.rdma = {
        .remote_addr = raddr,
        .rkey = rkey,
    },
};

struct ibv_send_wr *bad_wr;
ibv_post_send(qp, &wr, &bad_wr);

// Poll for completion
struct ibv_wc wc;
while (ibv_poll_cq(cq, 1, &wc) == 0) {
    // Wait
}

if (wc.status != IBV_WC_SUCCESS) {
    fprintf(stderr, "RDMA write failed\n");
}
```

---

## 4. RDMA on Loopback (SoftROCE)

### Setup SoftROCE

```bash
# Load RXE module
$ sudo modprobe rdma_rxe

# Add RXE device on lo interface
$ sudo rdma link add rxe0 type rxe netdev lo

# Verify
$ rdma link
link rxe0/1 state ACTIVE physical_state LINK_UP netdev lo

$ ibv_devices
    device          node GUID
    ------          ---------
    rxe0            505400fffef6f6f6
```

### Test with rping

```bash
# Terminal 1: Server
$ rping -s -v

# Terminal 2: Client
$ rping -c -a 127.0.0.1 -v
```

---

## 5. Why RDMA is Faster

### Latency Comparison

```
Operation          Socket      RDMA
──────────────────────────────────────
Small message      10-50 μs    1-2 μs
Context switch     Yes         No
Copies             2           0
CPU per message    High        Near zero
```

### Throughput Comparison

```
Socket (100 Gbps NIC): ~40 Gbps (CPU limited)
RDMA   (100 Gbps NIC): ~95 Gbps (line rate)
```

---

## 6. Practice Exercises

### Exercise 1: Setup SoftROCE

Configure RXE device and run ibv_devinfo to see attributes.

### Exercise 2: Measure Registration Cost

Time ibv_reg_mr for different buffer sizes. Plot the results.

### Exercise 3: Compare Latency

Implement simple ping-pong using:
1. UDP sockets
2. RDMA SEND/RECV
Compare latency distributions.

---

## Next Module

[Module 9: Maple Tree & VMA →](../module_09_maple_tree/)

[← Back to Course Index](../index.md)
