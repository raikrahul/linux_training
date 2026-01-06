:AXIOM 0: THE SETUP (NUMBERS)
01. `regs`: Address 1000 (Pointer to Struct).
02. `sizeof(struct pt_regs)`: 168 bytes.
03. `offset`: 8 bytes (Targeting the 2nd field).

:TRACE 1: THE WRONG WAY (POINTER MATH)
04. Expression: `regs + offset` (No Cast).
05. C Rule: `Pointer + N` adds `N * sizeof(Type)`.
06. Computation: `1000 + (8 * 168)`.
07. Result: `1000 + 1344 = 2344`.
08. Error: We jumped WAY past the struct. We wanted 1008.

:TRACE 2: THE RIGHT WAY (INTEGER MATH)
09. Expression: `(unsigned long)regs + offset`.
10. Action: Convert Pointer (1000) to Integer number (1000).
11. Computation: `1000 + 8`. (Simple Addition).
12. Result: `1008`.
13. Success: This is exactly 8 bytes forward.

:TRACE 3: THE ACCESS (RE-CAST)
14. We have Address 1008.
15. Expression: `(unsigned long *)1008`.
16. Meaning: "Trust me, there is an unsigned long at address 1008".
17. Dereference `*`: Read 8 bytes starting at 1008.

:CONCLUSION
18. The cast to `unsigned long` forces the compiler to switch from "Smart Step" (Struct Size) to "Baby Step" (Bytes). ✓
