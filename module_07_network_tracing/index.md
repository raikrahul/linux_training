# RDMA Basics — Network Copy Tracing

Axiomatic derivation of Linux network data copies. Kernel module tracing. Zero hand-waving.

## Files

| File | Purpose |
|------|---------|
| [axiomatic_derivation.md](axiomatic_derivation.md) | 20-step derivation from bits to double-copy proof |
| [packet_receive_axioms.md](packet_receive_axioms.md) | Packet RX flow, socket lookup proof from kernel source |
| [worksheet.md](worksheet.md) | COPY #1-#4 proofs with real machine addresses |
| [exercises_grilling.md](exercises_grilling.md) | 72 grilling questions, paradoxes, failure predictions |

## Code

| File | Purpose |
|------|---------|
| [sender.c](sender.c) | UDP sender, prints buffer VA |
| [receiver.c](receiver.c) | UDP receiver, prints buffer VA before/after recv() |
| [send_trace_hw.c](send_trace_hw.c) | Kernel module, kprobe on `_copy_from_iter` (COPY #1) |
| [recv_trace_hw.c](recv_trace_hw.c) | Kernel module, kprobe on `_copy_to_iter` (COPY #4) |

## Build

```bash
make          # builds sender, receiver, kernel modules
sudo insmod send_trace_hw.ko
sudo insmod recv_trace_hw.ko
./receiver &
./sender
sudo dmesg | grep COPY
```

## Proven Data Flow

```
User (sender) → COPY #1 → Kernel skb → COPY #4 → User (receiver)
     ↓                        ↓                       ↓
0x618d9aaa3069         0xffff8882cbbe612c      0x7fff43abc810
```

---

## Error Report

ERROR 1: line 122-125 of recv_trace_hw.c → accessed `iter->iter_type` before null check → should check `if (!iter)` FIRST → sloppy: assumed pointer valid → missed: pointer can be NULL → prevent: always null-check before any dereference

ERROR 2: line 128 of recv_trace_hw.c → null check placed AFTER dereference → should be BEFORE → sloppy: copy-paste without order check → missed: C evaluates left-to-right → prevent: read code top-to-bottom before commit

ERROR 3: worksheet.md line 602 → wrote "NEW THINGS INTRODUCED: NONE" without verifying → should list each term and trace origin → sloppy: assumed correctness → missed: axiomatic rule 21 → prevent: run mental checklist at end

ERROR 4: used `in_interrupt()` check assuming recv() might be in IRQ → recv() is syscall, runs in process context → should not filter with in_interrupt() → sloppy: over-defensive → missed: understand call context → prevent: read kernel docs before adding checks

ERROR 5: password typed wrong multiple times → command failed → should use correct password → sloppy: typing error → missed: verify before submit → prevent: slow down

ERROR 6: tried `git push` with SSH URL without SSH key configured → failed with "Permission denied" → should use HTTPS URL → sloppy: didn't check remote URL → missed: verify remote works before push → prevent: `git remote -v` before push

ERROR 7: hash_test.c calculation → assumed `net_seed = 0` → real kernel uses `net_hash_mix(net)` which is non-zero → should note assumption explicitly → sloppy: simplified without marking → missed: real value unknown → prevent: always mark assumptions with "ASSUMED"

ERROR 8: worksheet.md AXIOM BLOCK numbers → jumped from 18 to 19 without checking continuity → should verify previous block number → sloppy: assumed sequential → missed: count before assign → prevent: grep for last block number

---

## Orthogonal Thoughts

Q: Why did I access iter before checking null?
A: Brain wanted result first, safety second. Invert: safety first, result second.

Q: Why did I add in_interrupt() without understanding?
A: Brain copied pattern from other code. Invert: understand before copy.

Q: Why did I use SSH URL?
A: Brain remembered old config. Invert: check current state, not memory.

Q: Why did I assume net_seed = 0?
A: Brain wanted simple example. Invert: mark assumptions, don't hide.

---

## Kernel Source References

- `/home/r/Desktop/learn_kernel/source/net/ipv4/udp.c` line 528: `__udp4_lib_lookup()`
- `/home/r/Desktop/learn_kernel/source/include/net/ip.h` line 696: `ipv4_portaddr_hash()`
- `/home/r/Desktop/learn_kernel/source/include/linux/jhash.h` line 171: `jhash_1word()`
- `/home/r/Desktop/learn_kernel/source/lib/iov_iter.c` line 259: `_copy_to_iter()`
