---
title: "TLB and PCID"
difficulty: Advanced
order: 4
---

# TLB and PCID/ASID

Translation Lookaside Buffer caches virtual-to-physical translations.

## TLB Miss Cost

```
Without TLB (cold miss):
  PML4 read: 100 ns
  PDPT read: 100 ns
  PD read:   100 ns
  PT read:   100 ns
  ─────────────────
  Total:     400 ns

With TLB hit:
  TLB lookup: ~1 ns
  ─────────────────
  Speedup:   400×
```

## TLB Coverage

| Level | Entries | Page Size | Coverage |
|-------|---------|-----------|----------|
| L1 dTLB | 64 | 4KB | 256 KB |
| L1 dTLB | 32 | 2MB | 64 MB |
| L2 TLB | 1536 | 4KB | 6 MB |

## PCID (Process Context ID)

```
CR3 with PCID:
┌────────────────────────────────────────┐
│ 63  │ 62:52 │ 51:12          │ 11:0   │
│ NF  │ Rsvd  │ PML4 phys addr │ PCID   │
└────────────────────────────────────────┘

NF = No Flush (1 = keep TLB entries on CR3 write)
PCID = 12 bits = 4096 possible contexts
```

## Without PCID

```
Context switch A→B:
  Write CR3 = B's PML4
  TLB FLUSHED!
  All A's entries gone.
  
Context switch B→A:
  Write CR3 = A's PML4
  TLB FLUSHED again!
  Must re-walk all pages.
```

## With PCID

```
Context switch A→B:
  Write CR3 = B's PML4 | PCID_B | (1<<63)
  TLB NOT flushed!
  A's entries kept (tagged with PCID_A)
  
Context switch B→A:
  A's TLB entries still valid!
  No page walks needed.
```
