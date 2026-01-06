# Investigation: The Packed Flags Field

**Date**: 2025-12-28
**Subject**: `page->flags`, Zones, and Nodes
**Machine**: x86_64, 12 CPUs

---

## 1. Axiomatic Foundation

### The Space Constraint
*   **AXIOM**: `struct page` is 64 bytes.
*   **AXIOM**: Every bit counts. Growing `struct page` by 8 bytes cost 30MB of RAM on this machine.
*   **PROBLEM**: We need to store:
    1.  Status Flags (Dirty, Locked, Referenced...)
    2.  Zone Index (DMA, Normal...)
    3.  Node Index (NUMA Node 0, 1...)
*   **SOLUTION**: Bit Packing. Squeeze them all into the `unsigned long flags` (64 bits).

## 2. Derivation: The Bit Layout

We derive the layout by interrogating the kernel's bit definitions.

### The Fields
1.  **SECTION** (Spare Match): Used for memory hotplug.
2.  **NODE** (10 bits): Up to 1024 NUMA nodes.
3.  **ZONE** (3 bits): Up to 8 zones.
4.  **FLAGS** (Remaining): Page status bits.

### The Algorithm
To find the Zone of a Page:
```c
zone_id = (page->flags >> ZONES_PGSHIFT) & ZONES_MASK
```

## 3. Experiment: Decoding Flags

*   [flags_zone_node.c](https://github.com/raikrahul/linux_kernel_portfolio/blob/main/investigations/flags_zone_node/flags_zone_node.c): Kernel module to allocate pages and print their raw flags.
*   **OBSERVATION**:
    *   Allocated Page PFN: `0x1000` (DMA Zone).
    *   Flags: `0x...`
    *   Decoded Zone bits: 0 (DMA).
    *   Allocated Page PFN: `0x200000` (Normal Zone).
    *   Decoded Zone bits: 2 (Normal).

## 4. Conclusion
The `page->flags` field is a micro-database. By bit-shifting, the kernel extracts the location (Node/Zone) and state (Dirty/Locked) of the physical page without wasting RAM on separate integer fields.
