# GRILLING THE TASK: DECODING PAGE->MAPPING

## PHASE 1: AXIOMATIC INTERROGATION
We must question every single word in the requirement. We accept nothing. We calculate everything.

### 1. THE HARDWARE REALITY OF POINTERS
**Task:** "The lower 2 bits are given over to flags."
**GRILL:**
1.  Why 2 bits? Why not 3? Why not 1?
2.  What dictates the value of a pointer?
3.  **CALCULATION REQUIRED:**
    *   What is `alignof(struct address_space)`?
    *   What is `alignof(struct anon_vma)`?
    *   What is `alignof(struct movable_operations)`?
    *   If alignment is N bytes, how many lower bits are *guaranteed* to be zero?
    *   Formula: `Bits_Available = log2(Alignment)`
    *   Prove this for x86_64 architecture.
4.  **FAILURE PREDICTION:** What happens if a pointer is *not* aligned? (e.g. `__packed` struct). Would the kernel crash?

### 2. THE BITWISE MECHANICS
**Task:** "PAGE_MAPPING_ANON flag is set alone"
**GRILL:**
1.  What is the numerical value of this flag? (Source file lookup required).
2.  How do we *physically* combine a pointer and a flag?
    *   Operation: `Pointer | Flag`? `Pointer + Flag`? Are they identical if bits are zero?
3.  How do we *extract* the pointer back?
    *   Operation: `Value & Mask`.
    *   What is the mask? Derivation from `~0x3`?
    *   **CALCULATION REQUIRED:**
        *   Show masking of `0xffff888000000001` with `0x3` and `~0x3` in binary.
        *   Show what happens if you dereference the raw value `0xffff888000000001`. (CPU Exception #GP?)

### 3. THE FOUR STATES
**Task:** "Page Cache", "Anon", "Movable", "KSM".
**GRILL:**
1.  These correspond to the 2 bits. 2 bits = 4 combinations.
    *   `00` -> ?
    *   `01` -> ?
    *   `10` -> ?
    *   `11` -> ?
2.  **CALCULATION REQUIRED:**
    *   Define the exact mapping of bits to types.
    *   Is KSM `11` or `10`? Text says "combination of prior two". If Anon=1, Movable=2, then KSM=3?
    *   Verify against kernel source macros.

### 4. THE STRUCTS (THE "WHAT")
**Task:** "points to struct address_space", "struct anon_vma", etc.
**GRILL:**
1.  We need to verify these structs exist and are valid targets.
2.  **CALCULATION REQUIRED:**
    *   What is `sizeof(struct address_space)`?
    *   What is `sizeof(struct anon_vma)`?
    *   Does their alignment support the 2-bit assumption?

### 5. THE TRIGGER (THE "HOW")
**Task:** "Demo that". We need to make the machine create these states.
**GRILL:**
1.  **State 0 (Page Cache):** How to force a page into page cache?
    *   `mmap` a file? `read` a file?
2.  **State 1 (Anon):** How to force a page into Anon?
    *   `malloc` + usage? `mmap(MAP_ANONYMOUS)`?
    *   Does it happen immediately or on page fault?
3.  **State 2 (Movable):** What is this?
    *   Is it for device memory? Balloon drivers?
    *   Can we trigger this easily from userspace? Or do we mock it?
4.  **State 3 (KSM):** How to force KSM?
    *   `madvise(MADV_MERGEABLE)`?
    *   Needs `ksmd` thread to run? How long to wait?
    *   How to force deduplication of two pages?

## PHASE 2: IMPLEMENTATION GRILLING

### 1. THE USERSPACE TRIGGER (`mapping_user.c`)
**Task:** Generate pages in distinct states.
**GRILL:**
1.  How do we get the *same* process to hold pages in different states?
2.  How do we pass *multiple* VAs to the kernel module?
3.  **PLAN:**
    *   Alloc 1: File-backed mmap (State 0).
    *   Alloc 2: Anon mmap (State 1).
    *   Alloc 3: KSM trigger (State 3).
    *   Alloc 4: (Movable? Skip if too hard/device specific?).

### 2. THE KERNEL INSPECTOR (`mapping_hw.c`)
**Task:** Read `page->mapping`.
**GRILL:**
1.  We cannot just read `page->mapping`. It's a pointer.
2.  **DANGER:** If we print `page->mapping` as `%px`, we see the raw value (with flags).
3.  **DANGER:** If we dereference `page->mapping` without masking, we crash.
4.  **CALCULATION REQUIRED:**
    *   Read raw unsigned long value.
    *   Extract bottom 2 bits -> determine TYPE.
    *   Mask bottom 2 bits -> get POINTER.
    *   Print both.

## PHASE 3: THE "TRAP" GRILLING
**Task:** "What parts are designed to trip me off?"
**GRILL:**
1.  **The "Pointer is an Int" Trap:**
    *   C treats `page->mapping` as `struct address_space *`.
    *   If you access `page->mapping->host`, the compiler generates code assuming a valid pointer.
    *   BUT the pointer has dirty bits (1, 2, or 3).
    *   **CPU TRAP:** Dereferencing `0x...1` is unaligned or non-canonical? Or just wrong address?
    *   **TASK:** We must cast to `unsigned long` BEFORE bits operations.
2.  **The "Type Confusion" Trap:**
    *   Once stripped, what is the pointer?
    *   If bits=0, it's `address_space*`.
    *   If bits=1, it's `anon_vma*`.
    *   If bits=3, it's `ksm_stable_node*`.
    *   **TASK:** We cannot dereference the stripped pointer unless we cast it to the *correct* struct type.
    *   (We might not be able to define `anon_vma` in a module if headers are private? Need to check).

## PHASE 4: EXECUTION PLAN (NO CODE YET)
1.  **Define Axioms:** Verify alignment logic mathematically.
2.  **Verify Macros:** Check `PAGE_MAPPING_ANON` values in source.
3.  **Construct Triggers:** Write userspace execution flow.
4.  **Construct Inspector:** Write kernel logic flow (Step-by-step extraction).
5.  **Predict Output:** Write down expected bits for each case.

DO NOT START CODING UNTIL AXIOMS ARE PROVEN.
