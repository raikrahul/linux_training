# Module 8: RDMA Fundamentals

## Overview

This module introduces Remote Direct Memory Access (RDMA), a technology that eliminates CPU-mediated data copies. You will understand how RDMA achieves zero-copy networking.

## Learning Objectives

By the end of this module, you will be able to:

1. Explain the RDMA architecture
2. Understand memory registration with ibv_reg_mr
3. Describe queue pairs and completion queues
4. Compare RDMA vs traditional TCP performance
5. Identify use cases for RDMA

## Key Concepts

### Why RDMA?

Traditional networking:
```
User → CPU copy → Kernel → NIC DMA → Wire
Wire → NIC DMA → Kernel → CPU copy → User
```

RDMA:
```
User → NIC DMA → Wire
Wire → NIC DMA → User
```

Zero CPU copies. Lower latency. Higher throughput.

### Memory Registration

Before RDMA can access memory, it must be registered:

```c
struct ibv_mr *mr = ibv_reg_mr(pd, buffer, size, access_flags);
```

Registration:
1. Pins pages in RAM (no swap)
2. Provides NIC with physical addresses
3. Returns lkey/rkey for remote access

### Queue Pairs (QP)

RDMA uses queue pairs for communication:

```
┌─────────────────┐
│   Send Queue    │ ← Post send work requests
├─────────────────┤
│ Receive Queue   │ ← Post receive buffers
└─────────────────┘
         ↓
┌─────────────────┐
│ Completion Queue│ ← Poll for completions
└─────────────────┘
```

### RDMA Operations

| Operation | Description |
|-----------|-------------|
| SEND | Send data to remote receive buffer |
| RECV | Receive data into local buffer |
| WRITE | Write to remote memory (no remote CPU) |
| READ | Read from remote memory (no remote CPU) |

## Hands-On Files

| File | Description |
|------|-------------|
| `worksheet.md` | RDMA concepts and setup |
| `axioms.md` | Fundamental RDMA principles |
| `proof.md` | Zero-copy verification |
| `code/rdma_loopback.c` | Loopback RDMA example |

## Prerequisites

- Module 7: Network Stack Tracing
- Understanding of DMA
- InfiniBand/RoCE hardware (optional for concepts)

## Next Module

[Module 9: Maple Tree & VMA →](../module_09_maple_tree/)
