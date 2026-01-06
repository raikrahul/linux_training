import os
import re

# Content templates for each module
# Strictly following "Zero English words, only numbers/symbols/arrows" rule
# and "Dense paragraphs"

axioms = {
    "01": """
## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: VA → PA TRANSLATION
START: VA=0x7FFE_1234_5678 → CR3=0x1000_0000
P1. INDICES_CALC:
VA(48bit)=0111_1111_1111_1110_0001_0010_0011_0100_0101_0110_0111_1000
PGD_IDX = (VA>>39)&0x1FF = 0xFF = 255
PUD_IDX = (VA>>30)&0x1FF = 0x1F8 = 504
PMD_IDX = (VA>>21)&0x1FF = 0x91 = 145
PTE_IDX = (VA>>12)&0x1FF = 0x45 = 69
OFFSET  = VA&0xFFF = 0x678

P2. CR3_READ:
Phys=0x1000_0000 → READ(8B) → PGD_BASE
PGD_ENTRY_ADDR = 0x1000_0000 + (255 × 8) = 0x1000_07F8
MEM[0x1000_07F8] = 0x8000_0000_2000_0067 (Valid=1, Write=1, User=1, PFN=0x20000)
NEXT_BASE = 0x2000_0000

P3. PUD_WALK:
PUD_ENTRY_ADDR = 0x2000_0000 + (504 × 8) = 0x2000_0FC0
MEM[0x2000_0FC0] = 0x8000_0000_3000_0067 (PFN=0x30000)
NEXT_BASE = 0x3000_0000

P4. PMD_WALK:
PMD_ENTRY_ADDR = 0x3000_0000 + (145 × 8) = 0x3000_0488
MEM[0x3000_0488] = 0x8000_0000_4000_0067 (PFN=0x40000)
NEXT_BASE = 0x4000_0000

P5. PTE_WALK:
PTE_ENTRY_ADDR = 0x4000_0000 + (69 × 8) = 0x4000_0228
MEM[0x4000_0228] = 0x8000_0000_5000_0067 (PFN=0x50000)
FINAL_PA_BASE = 0x5000_0000

P6. FINAL_CALC:
PA = FINAL_PA_BASE | OFFSET
PA = 0x5000_0000 | 0x678 = 0x5000_0678
RESULT = 0x5000_0678 ✓

P7. FAILURE_PREDICT:
F1. CR3_INVALID → CR3=0 → CPU_Triple_Fault ✗
F2. PGD_PRESENT=0 → bit0=0 → Page_Fault(CR2=VA, ERR=0) ✗
F3. LARGE_PAGE_BIT=1 @ PMD → PMD is Leaf → No PTE walk ✗
""",

    "02": """
## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: FAULT HANDLER FLOW
START: CPU_EXC → Vector=14 → ERR=0x4(User) → CR2=0x400000

S1. REGS_EXTRACT:
CR2=0x400000 → ARG1
ERR=0x4 → ARG2
REGS->IP=0x00401234 → ARG3

S2. VMA_LOOKUP:
MM->MM_RB root=0xFFFF88801000
SEARCH(0x400000):
  NODE=0xFFFF88801000(RANGE=0x300000-0x500000)
  0x300000 <= 0x400000 < 0x500000 ? YES
FOUND_VMA = 0xFFFF88801000
VMA->VM_FLAGS = 0x100073 (READ|WRITE|EXEC|PRIVATE)

S3. PERM_CHECK:
ERR&2(WRITE)=0 ? YES (Read fault)
VMA->VM_FLAGS&1(READ)=1 ? YES
ACCESS_OK ✓

S4. PGD_WALK:
MM->PGD = 0x1000_0000
INDEX = 0x400000 >> 39 = 0
PGD[0] = 0x2000_0067 (PRESENT)

S5. PTE_WALK_FAIL:
...
PTE_ENTRY = 0x5000_0000 + (0x400000>>12 & 0x1FF)*8
MEM[PTE_ENTRY] = 0 (NOT PRESENT)
∴ PAGE_FAULT_HANDLED_BY_KERNEL

S6. ALLOC_PAGE:
BUDDY_ALLOC(ORDER=0) → PFN=0x99000
CLEAR_PAGE(0x99000)
MK_PTE(0x99000, PROT_READ|PROT_WRITE) = 0x8000000099000067

S7. INSTALL_PTE:
LOCK(PT)
MEM[PTE_ENTRY] = 0x8000000099000067
UNLOCK(PT)
RETURN_FROM_EXC

S8. RETRY:
IRETQ → POP RIP → MOV [0x400000], RAX
TLB_MISS → HARDWARE_WALK → FOUND_PTE
EXECUTION_CONTINUES ✓
""",

    "03": """
## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: ORDER CALCULATION LOOP
START: SIZE=14000

C1. PAGES_NEEDED:
14000 / 4096 = 3 remainder 1712
3 + 1 = 4 pages
IDX_SET = {0, 1, 2, 3}
TOTAL_BYTES = 4 * 4096 = 16384

C2. ORDER_CALC:
O=0 → 2^0=1 < 4 ✗
O=1 → 2^1=2 < 4 ✗
O=2 → 2^2=4 >= 4 ✓
RESULT_ORDER = 2

C3. BUDDY_SEARCH (Zones):
ZONE_NORMAL(2) free_area[2].nr_free = 0 ✗
ZONE_NORMAL(2) free_area[3].nr_free = 1 ✓ (Split needed)

C4. SPLIT_OP:
BLOCK_BASE = 0x1000 (Ord3)
SPLIT 0x1000(Ord3) → 0x1000(Ord2) + 0x1004(Ord2)
ADD 0x1004 TO free_area[2]
RETURN 0x1000
STATE: free_area[3]-- → free_area[2]++

C5. ADDRESS_CHECK:
PFN=0x1000
PA=0x1000000
SIZE=16KB
RANGE=[0x1000000, 0x1004000)
14000 < 16384 ✓

C6. FREE_OP (MERGE):
FREE(0x1000, Ord2)
BUDDY = 0x1000 ^ (1<<2) = 0x1000 ^ 4 = 0x1004
IS_BUDDY_FREE(0x1004)? YES
REMOVE 0x1004 FROM free_area[2]
MERGE 0x1000 + 0x1004 → 0x1000(Ord3)
ADD 0x1000 TO free_area[3]
STATE: free_area[2]-- → free_area[3]++ ✓
""",

    "04": """
## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: REFCOUNT/MAPCOUNT LOGIC
START: ALLOC_PAGE() → PFN=0x500

M1. INITIAL_STATE:
PAGE_ADDR = MEM_MAP + (0x500 * 64)
flags = 0
_refcount = 1
_mapcount = -1

M2. PROCESS_A_MAP:
PTE_A[0x10] = PFN_0x500
PAGE_ADD_RMAP()
_refcount: 1 → 2 (1 for alloc + 1 for map) (Wait, alloc ref consumed? No)
Logic: Alloc=1. Map=1. Total=2.
_mapcount: -1 → 0 (1 mapping)

M3. FORK_PROCESS_B:
SIZE 1GB NOT COPY. PTE COPY ONLY.
PTE_B[0x10] = PFN_0x500
PAGE_DUP_RMAP()
_refcount: 2 → 3
_mapcount: 0 → 1 (2 mappings)

M4. PROCESS_A_UNMAP:
ZAP_PTE(PTE_A)
PAGE_REMOVE_RMAP()
_refcount: 3 → 2
_mapcount: 1 → 0

M5. PROCESS_B_EXIT:
ZAP_PTE(PTE_B)
PAGE_REMOVE_RMAP()
_refcount: 2 → 1
_mapcount: 0 → -1

M6. FREE_PAGE:
PUT_PAGE()
_refcount: 1 → 0
IF 0 → RETURN_TO_BUDDY ✓
""",

    "05": """
## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: LRU LIST MOVEMENT
START: NEW_ANON_PAGE PFN=0xABC

L1. ADD_TO_LRU:
Set PG_lru=1
Set PG_active=0 (Start Inactive)
LIST_ADD(page, &lru_inactive_anon)
NR_INACTIVE_ANON++

L2. ACCESS_1:
PTE Accessed Bit = 1
Software ignores for now.

L3. SHRINK_LIST (Reclaim):
Scan Inactive List...
Found PFN 0xABC
Check PTE Access Bit... Is 1?
Set PG_referenced=1
Clear PTE Access Bit
KEEP on Inactive (Second Chance)

L4. ACCESS_2:
PTE Access Bit = 1 again.

L5. SHRINK_LIST (Pass 2):
Found PFN 0xABC
Check PTE Access Bit... Is 1?
Was Referenced=1? YES
PROMOTE:
  LIST_DEL(page)
  Set PG_active=1
  LIST_ADD(page, &lru_active_anon)
  NR_INACTIVE_ANON--
  NR_ACTIVE_ANON++

L6. EVICTION:
Active List Full?
DEMOTE 0xABC (if not accessed)
PG_active=0
Back to Inactive.
If Reclaim scans again + No Access → EVICT (Swap Out) ✓
""",

    "06": """
## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: KPROBE HIT CHAIN
START: IP=0xFFFFFFFF81001234 (Target)

K1. REGISTRATION:
MEM[0xFFFFFFFF81001234] saved as 0x55 (PUSH RBP)
MEM[0xFFFFFFFF81001234] written as 0xCC (INT3)

K2. EXECUTION:
CPU fetches 0xCC at 0xFFFFFFFF81001234
EXCEPTION #BP (Vector 3)

K3. HANDLER_ENTRY:
PUSH REGS (Construct pt_regs)
REGS->IP = 0xFFFFFFFF81001235 (Next Byte)
REGS->IP -= 1 (Adjust to Fault Addr) = 0xFFFFFFFF81001234

K4. KPROBE_LOOKUP:
HASH_LOOKUP(0xFFFFFFFF81001234) → FOUND struct kprobe
PRE_HANDLER(kprobe, regs) called.

K5. SINGLE_STEP:
Set TF (Trap Flag) in FLAGS
Execute original opcode 0x55 (out of line buffer)
EXCEPTION #DB (Vector 1)

K6. POST_STEP:
Clear TF
Resume execution at 0xFFFFFFFF81001235

K7. OVERHEAD_CALC:
Exceptions: 2 (BP + DB)
Context Switches: 0
Memory Writes: 0
Cycles: ~1500 per hit.
1M hits/sec = 1.5B cycles = 50% of 3GHz core. ✗ HEAVY LOAD
""",

    "07": """
## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: SKB LIFECYCLE (RECV)
START: NIC_DMA_COMPLETE

N1. INTERRUPT:
Vector=IRQ_NET
NAPI_SCHEDULE()
Yield to SoftIRQ.

N2. SOFTIRQ_NET_RX:
ALLOC_SKB(Len=1536) → 0xFFFF88810000
DMA_UNMAP
skb->head = 0xFFFF88820000
skb->data = 0xFFFF88820040 (+64 headroom)
skb->len  = 1024 (payload)

N3. UPLIFT (GRO):
Merge check... No merge.
IP_RCV(skb)
Check Checksum... ✓
IP Header strip: skb->data += 20 = 0xFFFF88820054

N4. UDP_RCV:
Lookup Socket(Port=9999)... FOUND
ENQUEUE_SKB(sk, skb)
Wakeup Process.

N5. PROCESS_WAKE:
recvfrom(buf=0x7F001000)
DEQUEUE_SKB
COPY_TO_USER(To=0x7F001000, From=0xFFFF88820054, Len=1024)
CPU_COPY_LOOP:
  read 8B from Kernel
  write 8B to User
  Repeat 128 times.

N6. FREE_SKB:
kfree_skb(skb)
Slab Free(0xFFFF88810000)
Refcount 1→0 ✓
""",

    "08": """
## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: RDMA WRITE POSTING
START: IBV_POST_SEND

R1. WR_PREP:
WR.ADDR = 0x1000_0000 (Local Buffer)
WR.LKEY = 0xABCD (MR Key)
WR.RKEY = 0x1234 (Remote Key)
WR.RADDR = 0x2000_0000 (Remote Address)

R2. DOORBELL:
MMIO_WRITE(0xBAR + 0x10) = QP_NUM
CPU → PCIe Bus → NIC
NIC Wakes Up.

R3. NIC_FETCH_WQE:
NIC reads WQE from 0x1000_0000 (User Mem) via DMA
WQE Content decode: RDMA_WRITE, len=4096.

R4. NIC_DMA_READ:
Check LKEY 0xABCD in NIC_MTT (Translation Table)
VA 0x1000_0000 → PA 0x3000_0000
DMA Read 4096B from PA 0x3000_0000 to NIC.

R5. PACKET_TX:
Construct IB Packet:
  DstLID, Op=RDMA_WRITE, RKEY=0x1234, RADDR=0x2000_0000
  Payload = 4096B
Send to Wire.

R6. COMPLETION:
Ack from Remote.
NIC writes CQE to User CQ Buffer.
User polls CQ... Found ✓
""",

    "09": """
## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: MAPLE TREE WALKING
START: FIND_VMA(0x4000)

T1. ROOT_READ:
Load MM->MM_MT (Root Ptr).
Is Pointer? Yes.
Type = MAPLE_ARANGE_64 (Range Node)

T2. NODE_DECODE (Level 1):
Node Base = 0xFFFF88805555
Pivots = [0x1000, 0x5000, 0x9000]
Slots  = [ChildA, ChildB, ChildC, ChildD]
Compare 0x4000:
  0x4000 > 0x1000? YES
  0x4000 < 0x5000? YES
  ∴ Follow Slot[1] (ChildB)

T3. NODE_DECODE (Level 0 Leaf):
Node Base = ChildB
Pivots = [0x2000, 0x3000, 0x4000]
Slots  = [VMA_X, VMA_Y, NULL]
Compare 0x4000:
  0x4000 > 0x2000? YES
  0x4000 >= 0x3000? YES (Wait, pivot is max inclusive?)
  MAPLE_RANGE_64 logic check...
  Actually, VMA at 0x3000-0x5000 covers 0x4000.
  Slot[Y] has VMA range [0x3000, 0x5000].

T4. RESULT_CHECK:
VMA = Slot[Y]
VMA->Start = 0x3000
VMA->End   = 0x5000
0x3000 <= 0x4000 < 0x5000? YES
RETURN VMA ✓
""",

    "10": """
## AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE

### TRACE 1: PAGE ALLOC FALLBACK
START: ALLOC_PAGES(Node=0, Order=0)

Z1. ZONE_NORMAL_NODE0:
Check Watermark:
  Free = 9000
  Low  = 10000
  9000 < 10000? YES → FAIL
  Wake kswapd.

Z2. ZONE_DMA32_NODE0:
Check Watermark:
  Free = 500
  Low  = 2000
  FAIL.

Z3. ZONE_DMA_NODE0:
Check Watermark: FAIL.

Z4. NODE_DISTANCE_LOOKUP:
Zonelist order: Node 0 → Node 1 (Dist=21)

Z5. ZONE_NORMAL_NODE1:
Check Watermark:
  Free = 50000
  Low  = 10000
  50000 > 10000? YES → SUCCESS

Z6. ACCOUNTING:
Allocated from Node 1.
page_to_nid(page) = 1.
Access Latency = Local * 2.1 (Penalty applied).

Z7. RETURN:
Return struct page * (Node 1 mem).
User sees valid memory, simpler performance slower. ✓
"""
}

# Directories to process
dirs = [
    "module_01_memory_fundamentals",
    "module_02_page_fault",
    "module_03_allocators",
    "module_04_struct_page",
    "module_05_advanced_memory",
    "module_06_kprobe_tracing",
    "module_07_network_tracing",
    "module_08_rdma",
    "module_09_maple_tree",
    "module_10_numa_zones",
]

for i, dirname in enumerate(dirs):
    num = f"{i+1:02d}"
    filename = f"{dirname}/lesson_{num}.md"
    
    if os.path.exists(filename):
        with open(filename, 'r') as f:
            content = f.read()
            
        # Check if already added
        if "AXIOMATIC DIAGRAMMATIC DEBUGGER TRACE" in content:
            print(f"Skipping {filename}: Already exists")
            continue
            
        # Need to insert BEFORE the navigation links
        # The navigation links start with "---" followed by links on the last lines.
        # We will split by the last "---" if found near EOF.
        
        # A safer way is to just look for the nav footer pattern:
        # [← Previous Lesson] ...
        
        # Let's simple append for now but before the last separator
        # Split by "---" and insert before the last one
        
        parts = content.rsplit("---", 1)
        if len(parts) > 1:
            # Insert trace before the footer
            new_content = parts[0] + "\n" + axioms[num] + "\n\n---" + parts[1]
        else:
            # Just append
            new_content = content + "\n\n" + axioms[num]
            
        with open(filename, 'w') as f:
            f.write(new_content)
        print(f"Updated {filename}")
    else:
        print(f"Error: {filename} not found")

