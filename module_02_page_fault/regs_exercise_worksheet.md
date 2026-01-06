1. Axiom: Size of `char` = 1 byte. Size of `long` = 8 bytes. Index range {0..N-1}. Binary representation 2^N.
2. Diagram: `struct mock_regs` layout in memory →
[Address 0x200 | field: rA (long)] →
[Address 0x208 | field: rB (long)] →
[Address 0x210 | field: rC (long)] →
[Address 0x218 | field: rD (long)] →
[Address 0x220 | field: rE (long)].
3. Calculation: Base = 0x200. Target = rD. Offset = Position(rD) - Position(rA). rA=0, rB=1, rC=2, rD=3. Offset = 3 × 8 bytes = 24 bytes (0x18). Logic: Base + Offset = 0x200 + 0x18 = 0x218. Result = 0x218 ✓.
4. Exercise: Fill table for Base = 0x500.
| Member | Index | Offset (dec) | Address (hex) |
|--------|-------|--------------|---------------|
| rA     | 0     | 0            | 0x500         |
| rB     | 1     | 8            | 0x508         |
| rC     | 2     | 16           | 0x510         |
| rD     | 3     | 24           | 0x518         |
| rE     | 4     | 32           | ______        |
Calculate rE Address. Verify: 0x500 + 32 = 0x520 ✓.
5. Puzzle: Target = rC. Pointer `p` = `(struct mock_regs *)0x500`. Incorrect Math: `p + 16` → 0x500 + (16 × 40) = 0x780 ✗. Correct Math: `(long)p + 16` → 0x500 + 16 = 0x510 ✓. Trick: Casting to `long` forces byte-step instead of struct-step.
6. Diagram: Memory State Transition →
Before: [0x510] = 0xDEADBEEF.
Action: `val = *(long *)((long)p + 16)`.
After: `val` == 0xDEADBEEF ✓.
7. Logic Chain: `regs_get_register(regs, offset)` →
`regs` (Input Base) →
`offset` (Input Displacement) →
`Base + Offset` (Target Address) →
`*Target` (Fetch Value).
8. Failure Prediction:
- F1: Adding offset 8 to `struct *` without cast → Result = Base + (8 * 168) → Out of bounds ✗.
- F2: Using 4-byte `int` for 8-byte `long` register → Result = Truncated 32-bits ✗.
- F3: Offset pointing to middle of register (e.g. 0x503) → Result = Misaligned Read Exception or Junk ✗.
9. Final Check: If Member = rB, Address = 0x508. `deref(0x508)` returns 8 bytes.
User: Calculate rE Address for Base 0x800. Fill: Offset_rE = ? Address_rE = ?. verify: (0x800+32)=0x820 ✓.
User: Draw 5 boxes representing mock_regs. Label addresses starting at 0x100. Draw arrow from Base+16 to the 3rd box.
User: Explain why adding 16 to `long *p` vs `long p` results in different Hex values.
10. FAILURE PREDICTIONS:
- ADHD-F1: User skips Step 3 calculation, treats 0x200 as 200 decimal → Calculation 200+24=224 is wrong address ✗.
- FOV-F1: User assumes registers are 4 bytes → Offset rC=8 instead of 16 → Reads partial rB data ✗.
- CAST-F1: User omits `(long *)` cast → Tries to dereference a raw integer → Segfault ✗.
