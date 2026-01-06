
# The PTE Mask Axiom: 000F FFFF FFFF F000

## 1. The Real PTE (64 Bits)
A Page Table Entry (PTE) is not just an address. It is a packed struct of 64 bits.
Imagine a 64-bit integer:

`Bit 63 ................................................. Bit 0`
`[ N ][ ...... Reserved ...... ][ PFN (Address) ][ Flags ]`

## 2. The Three Parts

### Part A: The Top (Bit 63) - "N"
- **Bit 63**: The "NX" (No Execute) bit.
- If this is `1`, verify code cannot run here.
- We MUST remove this bit to get the address.

### Part B: The Bottom (Bits 0-11) - "Flags"
- **Bits 0-11**: Permission flags (Read/Write, User/Kernel, Present, Accessed, Dirty).
- These are NOT part of the address.
- We MUST remove these bits.

### Part C: The Middle (Bits 12-51) - "The PFN"
- **Bits 12-51**: The actual Physical Frame Number.
- This is the number we want.
- It is 40 bits long (allowing up to 1 Terabyte of RAM with 4KB pages).

## 3. The Mask Derivation

We want to KEEP bits 12-51 and ZERO everything else.

| Bits Range | Purpose | We Want? | Hex Mask Digit |
| :--- | :--- | :--- | :--- |
| 63-52 | NX/Reserved | NO | `000` |
| 51-12 | **The PFN** | **YES** | `FFFFFFFFFF` (10 nibbles) |
| 11-0 | Flags | NO | `000` |

**Assemble the Hex:**
`0x 000 F FFFF FFFF F 000`

**Total Mask:** `0x000FFFFFFFFFF000`

## 4. The Shift Logic
After masking, we have: `0x0002c948c000`.
But this is still "shifted". It thinks it's a huge number.
The actual value is `0x2c948c`.

So we do `>> 12` to slide it down to the right.

`0x0002c948c000 >> 12` = `0x0000002c948c`.

**Now we have the clean integer PFN.**
