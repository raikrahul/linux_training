# Axiom: 52 - 12 = 40

## 1. The MAX Physical Address
Modern x86_64 CPUs support up to **52 Bits** of Physical RAM.
`2^52` = 4,503,599,627,370,496 Bytes (4 Petabytes).

## 2. The Page Offset (Fixed)
The bottom **12 Bits** are ALWAYS the "Byte Offset" inside the 4KB page.
(Because 4KB = 2^12).

## 3. The PFN (The Result)
If the Total Address is 52 bits, and the bottom 12 are Offset...
Then the **Page Number (PFN)** is the remaining top bits.

`52 (Total) - 12 (Offset) = 40 Bits (PFN)`

## 4. The Suitcase (PTE)
The PTE stores this **40-bit PFN** in bits 12-51.
That is why our mask `0x000FFFFFFFFFF000` has **10 F's**.
`10 * 4 bits = 40 bits`.

**Q: "40 or 52?"**
**A: The PFN is 40 bits. The Address is 52 bits.**
