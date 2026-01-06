---
title: "Virtual Memory Areas (VMA)"
difficulty: Intermediate
order: 5
---

# Virtual Memory Areas

VMAs describe contiguous virtual address regions.

## VMA Structure

```c
struct vm_area_struct {
    unsigned long vm_start;  // Start (inclusive)
    unsigned long vm_end;    // End (exclusive)
    unsigned long vm_flags;  // Permissions
    struct file *vm_file;    // Backing file
    ...
};
```

## Process Memory Layout

```
┌────────────────────────────────────────┐
│ 0x400000-0x401000  r-xp  /bin/cat      │ [.text]
│ 0x601000-0x602000  r--p  /bin/cat      │ [.rodata]
│ 0x602000-0x603000  rw-p  [heap]        │ anonymous
│ ...                                     │
│ 0x7ffc...-0x7fff0000  rw-p  [stack]    │ anonymous
└────────────────────────────────────────┘
```

## vm_flags

| Flag | Value | Meaning |
|------|-------|---------|
| VM_READ | 0x01 | Readable |
| VM_WRITE | 0x02 | Writable |
| VM_EXEC | 0x04 | Executable |
| VM_SHARED | 0x08 | Shared mapping |
| VM_GROWSDOWN | 0x100 | Stack |

## VMA vs PTE

```
VMA = INTENT
  "This region SHOULD be readable/writable"
  Virtual construct, no hardware meaning

PTE = CURRENT STATE
  "This page IS mapped to physical 0x12345"
  Hardware uses this for translation
```

## Size Calculation

```
vm_start = 0x7FFE5E400000
vm_end   = 0x7FFE5E500000
Size = vm_end - vm_start = 0x100000 = 1 MB
Pages = 1 MB / 4 KB = 256 pages
```
