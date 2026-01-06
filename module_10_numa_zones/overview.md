# Module 10: NUMA & Memory Zones

## Overview

This module covers Non-Uniform Memory Access (NUMA) architecture and Linux memory zones. You will understand how the kernel optimizes memory allocation for multi-socket systems.

## Learning Objectives

By the end of this module, you will be able to:

1. Explain NUMA architecture and memory localities
2. Describe Linux memory zones and their purposes
3. Understand zone fallback behavior
4. Analyze /proc/zoneinfo and /proc/buddyinfo
5. Configure NUMA policy for applications

## Key Concepts

### NUMA Architecture

In NUMA systems, memory access time depends on location:

```
┌─────────────────┐     ┌─────────────────┐
│   CPU Node 0    │     │   CPU Node 1    │
│   ┌─────────┐   │     │   ┌─────────┐   │
│   │ Local   │   │     │   │ Local   │   │
│   │ Memory  │   │     │   │ Memory  │   │
│   └────┬────┘   │     │   └────┬────┘   │
└────────┼────────┘     └────────┼────────┘
         │                       │
         └───────────┬───────────┘
                     │
              Interconnect
```

Local memory: ~80ns
Remote memory: ~150ns (depends on topology)

### Memory Zones

Linux divides physical memory into zones:

| Zone | Purpose | Typical Range |
|------|---------|---------------|
| ZONE_DMA | Legacy ISA DMA | 0-16MB |
| ZONE_DMA32 | 32-bit DMA | 0-4GB |
| ZONE_NORMAL | Regular kernel use | 4GB+ |
| ZONE_MOVABLE | Hotplug, migration | Configured |

### Zone Fallback

When a zone is exhausted, the allocator falls back:

```
ZONE_NORMAL exhausted
       ↓
Try ZONE_DMA32
       ↓
Try ZONE_DMA (if allowed)
       ↓
Trigger reclaim/OOM
```

### NUMA Policy

```c
// Allocate on local node
set_mempolicy(MPOL_LOCAL, NULL, 0);

// Interleave across nodes
set_mempolicy(MPOL_INTERLEAVE, nodemask, maxnode);
```

## Hands-On Files

| File | Description |
|------|-------------|
| `linuxMemoryLesson1.md` | NUMA basics |
| `derivation.md` | Zone calculations |
| `homework_worksheet.md` | Practice problems |
| `README.md` | Quick reference |

## Prerequisites

- Module 1: Memory Fundamentals
- Module 3: Memory Allocators
- Multi-socket system (optional)

## Course Complete

Congratulations! You have completed the Linux Kernel Training course.

[← Back to Course Index](../index.md)
