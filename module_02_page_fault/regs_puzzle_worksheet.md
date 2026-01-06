1. Base 0x1000. `long` = 8. `char` = 1. `struct` = r0..r9 (10 fields). Total = 80 bytes.
2. Index Set {r0:0, r1:1, r2:2, r3:3, r4:4, r5:5, r6:6, r7:7, r8:8, r9:9}.
3. Calc r3 Offset: Index(r3) × Size(long) → 3 × 8 = 24. (Hex: 0x18).
4. Address r3: Base + Offset → 0x1000 + 24 = 0x1018.
5. Pointer p = 0x1000. `p + 1` → 0x1000 + 80 = 0x1050 ✗ (Smart Math).
6. Pointer p = 0x1000. `(char *)p + 24` → 0x1000 + 24 = 0x1018 ✓ (Byte Math).
7. Pointer p = 0x1000. `(long)p + 24` → 0x1000 + 24 = 0x1018 ✓ (Integer Math).
8. Target Address T = 0x1018. Read `*(long *)T`.
9. Task: Fill table for Base 0x3000.
| Reg | Index | Offset | Calculation | Address |
|-----|-------|--------|-------------|---------|
| r0  | 0     | 0      | 0x3000+0    | 0x3000  |
| r1  | 1     | 8      | 0x3000+8    | 0x3008  |
| r5  | 5     | 40     | 0x3000+40   | ______  |
| r9  | 9     | 72     | 0x3000+72   | ______  |
Verify r5 = 0x3028 ✓. Verify r9 = 0x3048 ✓.
10. Puzzle: Base 0x4000. Read r2 AND r7. Sum addresses. (0x4010 + 0x4038) = 0x8048.
11. Logic: `regs_get_register` maps `Number` to `RAM Value`.
12. Failure Predictions:
- P1: Forget `(long)` cast → 0x1000 + 24 jumps to 0x1000 + 1920 ✗.
- P2: Miscount 1-based index → r3 = 4 * 8 = 32 bytes ✗.
- P3: Read 4 bytes from 8-byte field → Partial data ✗.
User: Draw 10 boxes (8 bytes each). Start address 0x100. Write address of each box. Draw arrow for `offset = 48`.
User: In `regs_puzzle.c`, implement the `get_reg` function using the `(long)` cast trick.
User: Verify `get_reg(regs, 24)` returns `r3` value.
