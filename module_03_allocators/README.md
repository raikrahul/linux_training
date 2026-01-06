# GFP CONTEXT BUG EXERCISE
## Axiomatic Derivation | Linux Kernel 6.14.0-37-generic | x86_64

---

### MACHINE DATA (AXIOMS)
```
Kernel: 6.14.0-37-generic
PAGE_SIZE: 4096
GFP_KERNEL: 0xCC0
GFP_ATOMIC: 0x0 (simplified, actually has other bits)
```

---

## PART 1: PREEMPT_COUNT BITS (DO BY HAND)

01. AXIOM: preempt_count() returns 32-bit integer.
02. AXIOM: Bits 0-7 = preemption depth. Range: 0-255.
03. AXIOM: Bits 8-15 = softirq count.
04. AXIOM: Bits 16-19 = hardirq count.
05. AXIOM: Bit 20 = NMI.
06. CALCULATE: Softirq mask = bits 8-15 = 0x___ (fill hex).
07. CALCULATE: Hardirq mask = bits 16-19 = 0x___ (fill hex).
08. CALCULATE: Interrupt mask = softirq | hardirq | NMI = 0x___ (fill hex).
09. FORMULA: in_interrupt() = (preempt_count() & interrupt_mask) != 0.

---

## PART 2: CONTEXT DETERMINATION (DO BY HAND)

10. SCENARIO A: preempt_count() = 0x00000000.
11. CALCULATE: 0x00000000 & 0x1FFF00 = ___.
12. RESULT: in_interrupt() = ___ (true/false).
13. CONTEXT: ___ context (process/interrupt).
14. GFP: Can use GFP_KERNEL? ___ (yes/no).

15. SCENARIO B: preempt_count() = 0x00000100.
16. CALCULATE: 0x00000100 & 0x1FFF00 = ___.
17. RESULT: in_interrupt() = ___ (true/false).
18. CONTEXT: ___ context (process/interrupt/softirq).
19. GFP: Can use GFP_KERNEL? ___ (yes/no).

20. SCENARIO C: preempt_count() = 0x00010000.
21. CALCULATE: 0x00010000 & 0x1FFF00 = ___.
22. RESULT: in_interrupt() = ___ (true/false).
23. CONTEXT: ___ context (hardirq).
24. GFP: Can use GFP_KERNEL? ___ (yes/no).

---

## PART 3: GFP FLAGS BITS (DO BY HAND)

25. AXIOM: ___GFP_IO_BIT = 6.
26. AXIOM: ___GFP_FS_BIT = 7.
27. AXIOM: ___GFP_DIRECT_RECLAIM_BIT = 10.
28. AXIOM: ___GFP_KSWAPD_RECLAIM_BIT = 11.
29. CALCULATE: __GFP_IO = 1 << 6 = ___.
30. CALCULATE: __GFP_FS = 1 << 7 = ___.
31. CALCULATE: __GFP_DIRECT_RECLAIM = 1 << 10 = ___.
32. CALCULATE: __GFP_KSWAPD_RECLAIM = 1 << 11 = ___.
33. CALCULATE: __GFP_RECLAIM = __GFP_DIRECT_RECLAIM | __GFP_KSWAPD_RECLAIM = ___ | ___ = ___.
34. CALCULATE: GFP_KERNEL = __GFP_RECLAIM | __GFP_IO | __GFP_FS = ___ | ___ | ___ = ___.
35. VERIFY: GFP_KERNEL = 0xCC0? ___ (✓/✗).

---

## PART 4: WHY GFP_KERNEL FAILS IN INTERRUPT

36. PROBLEM: alloc_page(GFP_KERNEL) in softirq.
37. GFP_KERNEL contains __GFP_DIRECT_RECLAIM = bit 10 = ___.
38. __GFP_DIRECT_RECLAIM means: kernel MAY call schedule() to wait for pages.
39. schedule() checks: in_atomic()? 
40. If in_atomic() = true AND schedule() called → BUG_ON("scheduling while atomic").
41. Timer callback runs in softirq → preempt_count() has bits 8-15 set.
42. in_interrupt() = true → atomic context.
43. schedule() called → BUG.

---

## PART 5: TRACE TIMER FLOW

44. module_init() runs in process context.
45. preempt_count() = 0x00000000 → in_interrupt() = false.
46. GFP_KERNEL safe here.
47. timer_setup() arms timer for 100ms later.
48. 100ms passes...
49. Timer fires → timer_callback() runs.
50. Now in softirq context → preempt_count() = 0x00000100 (example).
51. alloc_page(GFP_KERNEL) → checks if can reclaim.
52. __GFP_DIRECT_RECLAIM set → tries to sleep.
53. schedule() → BUG_ON(in_atomic()).

---

## PART 6: VERIFICATION COMMANDS

54. BEFORE: sudo insmod gfp_context_bug.ko
55. WAIT: 100ms for timer to fire.
56. CHECK: sudo dmesg | grep GFP_CONTEXT
57. EXPECTED OUTPUT:
    - init: in_interrupt()=0, preempt_count()=0x0
    - timer_callback: in_interrupt()=1, preempt_count()=0x??? (non-zero)
    - alloc_SUCCESS or alloc_FAILED (depending on memory pressure)
58. IF BUG LINE UNCOMMENTED: dmesg shows "BUG: scheduling while atomic" or similar.
59. sudo rmmod gfp_context_bug

---

## PART 7: FAILURE PREDICTIONS

F1. User uses GFP_KERNEL everywhere → works in init, crashes in timer.
F2. User confuses preempt_count bits → wrong mask calculation.
F3. User forgets timer runs async → expects sequential execution.
F4. User doesn't check alloc return → NULL dereference in interrupt = hard crash.
F5. User uses GFP_ATOMIC in process context → works but wastes emergency reserves.

---

## ANSWERS (FILL AFTER TRYING)

```
06. 0xFF00 (bits 8-15)
07. 0xF0000 (bits 16-19)
08. 0x1FFF00 (all interrupt bits)
11. 0x00000000
12. false
13. process
14. yes
16. 0x00000100
17. true
18. softirq
19. no
21. 0x00010000
22. true
23. hardirq
24. no
29. 64 = 0x40
30. 128 = 0x80
31. 1024 = 0x400
32. 2048 = 0x800
33. 0x400 | 0x800 = 0xC00
34. 0xC00 | 0x40 | 0x80 = 0xCC0
35. ✓
37. 0x400
```

---
