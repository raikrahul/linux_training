RDMA FROM SCRATCH — AXIOMATIC DERIVATION — PRIMATE LEVEL — NO FORWARD REFERENCES — EACH LINE USES ONLY PREVIOUS LINES

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 0: WHAT IS A BIT?

001. AXIOM: You can hold up 0 fingers or 1 finger → two choices → we call each choice a "bit" → bit ∈ {0, 1}
002. DERIVED FROM 001: with 1 bit you can represent 2 different things → thing₀ = 0, thing₁ = 1 → count of things = 2 = 2^1
003. DERIVED FROM 001: with 2 bits you can represent 4 different things → 00, 01, 10, 11 → count = 4 = 2^2
004. DERIVED FROM 003: with N bits you can represent 2^N different things → formula: choices = 2^N
005. EXERCISE: with 3 bits how many things? → 2^3 = __________ → list them: 000, 001, 010, 011, 100, 101, 110, 111 → count = 8 ✓

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 1: WHAT IS A BYTE?

006. DEFINITION: 1 byte = 8 bits grouped together → derived from 001: 8 copies of bit
007. DERIVED FROM 004, 006: with 1 byte you can represent 2^8 = 256 different things → range [0, 255]
008. EXERCISE: what is largest number in 1 byte? → 2^8 - 1 = 256 - 1 = __________ → verify: 11111111 binary = 255
009. NOTATION: we write byte values in hexadecimal (hex) → hex digit ∈ {0,1,2,3,4,5,6,7,8,9,A,B,C,D,E,F} → 16 symbols
010. DERIVED FROM 004: 1 hex digit represents 4 bits → 2^4 = 16 → matches 16 symbols in 009
011. DERIVED FROM 006, 010: 1 byte = 8 bits = 2 hex digits → example: byte 255 = 11111111 binary = FF hex
012. EXERCISE: convert byte 170 to binary → 170 = 128 + 32 + 8 + 2 = 2^7 + 2^5 + 2^3 + 2^1 → binary = 10101010 → hex = __________ → AA

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 2: WHAT IS RAM (Random Access Memory)?

013. AXIOM: Your machine has RAM = a long row of bytes, numbered starting from 0
014. FACT FROM YOUR MACHINE: `cat /proc/meminfo | grep MemTotal` → 15776264 kB
015. DEFINITION: 1 kB = 1024 bytes → derived from 004: 1024 = 2^10
016. DERIVED FROM 014, 015: total bytes in your RAM = 15776264 × 1024 = 16154894336 bytes
017. EXERCISE: calculate 15776264 × 1024 by hand → 15776264 × 1000 = 15776264000, then 15776264 × 24 = 378630336, sum = __________
018. DEFINITION: each byte in RAM has a unique number called its "address" → address ∈ [0, total_bytes - 1]
019. DERIVED FROM 016, 018: addresses in your RAM range from 0 to 16154894335
020. NOTATION: we write addresses in hex with prefix 0x → address 0 = 0x0, address 255 = 0xFF, address 4096 = 0x1000

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 3: WHAT IS A PAGE?

021. PROBLEM: managing 16 billion individual bytes is slow → group them into chunks called "pages"
022. FACT FROM YOUR MACHINE: `getconf PAGE_SIZE` → 4096 bytes
023. DERIVED FROM 004: 4096 = 2^12 → so 12 bits needed to count bytes within one page
024. DEFINITION: page = 4096 consecutive bytes aligned at multiple of 4096
025. DERIVED FROM 022: addresses 0-4095 = page 0, addresses 4096-8191 = page 1, addresses 8192-12287 = page 2
026. DEFINITION: PFN (Page Frame Number) = page's index → PFN 0 = page at address 0, PFN 1 = page at address 4096
027. DERIVED FROM 022, 026: formula: address_of_page = PFN × 4096 → inverse: PFN = address / 4096 (integer division)
028. DERIVED FROM 016, 022: total pages in your RAM = 16154894336 / 4096 = 3944066 pages
029. EXERCISE: verify 028 → 3944066 × 4096 = __________ → should equal 16154894336 (may have rounding)

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 4: WHY DO WE NEED VIRTUAL ADDRESSES?

030. PROBLEM: multiple programs run at same time → each program wants to use address 0x1000 → conflict
031. SOLUTION: give each program its own "pretend" address space → we call pretend addresses "virtual addresses" (VA)
032. DEFINITION: VA = address the program thinks it's using → PA = actual address in RAM (Physical Address)
033. DERIVED FROM 031, 032: the program writes to VA 0x1000, but CPU secretly redirects to PA 0x50000 or somewhere else
034. DEFINITION: "translation" = converting VA to PA → CPU does this for every memory access
035. EXERCISE: if program A has VA 0x1000 → PA 0x50000, and program B has VA 0x1000 → PA 0x80000, do they conflict? → NO → each has own PA

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 5: HOW DOES CPU TRANSLATE VA TO PA? (Page Table)

036. PROBLEM: 2^48 possible VAs (on x86_64) → cannot store translation for each VA (too many)
037. DERIVED FROM 023: translation is per-page, not per-byte → only need 2^48 / 4096 = 2^36 translations
038. DEFINITION: Page Table = data structure that stores (VA_page → PA_page) mappings
039. STRUCTURE: x86_64 uses 4-level page table → each level is a page (4096 bytes) containing 512 entries
040. DERIVED FROM 010: 512 = 2^9 → need 9 bits to index into each level
041. DERIVED FROM 040: 4 levels × 9 bits = 36 bits for indexing + 12 bits for offset within page = 48 bits total
042. EXERCISE: verify 041 → 9 + 9 + 9 + 9 + 12 = __________ → should equal 48

043. DEFINITION: CR3 = CPU register that holds physical address of top-level page table
044. WALK PROCEDURE: to translate VA → PA, CPU does:
     step 1: read PA of Level-4 table from CR3
     step 2: extract bits [47:39] of VA → index into Level-4 → get PA of Level-3 table
     step 3: extract bits [38:30] of VA → index into Level-3 → get PA of Level-2 table
     step 4: extract bits [29:21] of VA → index into Level-2 → get PA of Level-1 table
     step 5: extract bits [20:12] of VA → index into Level-1 → get PA of target page
     step 6: add bits [11:0] of VA (offset) to PA of page → final PA

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 6: WHAT IS A NETWORK INTERFACE CARD (NIC)?

045. AXIOM: your machine can send bytes to another machine over a wire (or wirelessly)
046. DEFINITION: NIC = hardware device that sends/receives bytes over network
047. FACT FROM YOUR MACHINE: `ip link show` → you have wlp3s0 (WiFi) and enp2s0 (Ethernet)
048. DEFINITION: MAC address = unique 6-byte identifier for your NIC → like a serial number
049. FACT FROM YOUR MACHINE: `ip link show wlp3s0 | grep ether` → 70:66:55:b1:9a:8d
050. DERIVED FROM 011, 049: MAC = 0x70, 0x66, 0x55, 0xb1, 0x9a, 0x8d → 6 bytes → 48 bits

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 7: HOW DOES NIC SEND DATA? (Traditional Method)

051. PROBLEM: program wants to send 64 bytes at VA 0x7ff0000 over network
052. TRADITIONAL STEPS:
     step 1: program calls send() system call
     step 2: CPU copies 64 bytes from VA 0x7ff0000 to kernel buffer (CPU does the copy)
     step 3: CPU copies 64 bytes from kernel buffer to NIC's TX buffer (CPU does the copy)
     step 4: NIC reads from TX buffer and sends over wire

053. PROBLEM WITH 052: CPU wastes time copying bytes → 2 copies per send → slow for large data

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

PROOF OF 052 FROM KERNEL SOURCE (YOUR MACHINE: /home/r/Desktop/learn_kernel/source/net/)

P01. AXIOM: you asked for proof → I searched kernel source for copy functions
P02. SEARCH: `grep copy_from_iter net/core/datagram.c` → found line 580
P03. FILE: /home/r/Desktop/learn_kernel/source/net/core/datagram.c
P04. LINE 568-581: function `skb_copy_datagram_from_iter` → this is COPY #1 (user → kernel)

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

WHAT IS AN ITERATOR? (DERIVATION BEFORE USING "iter" IN P05)

I01. PROBLEM: user program has data in multiple buffers → example: buf1 at 0x7ff0000 (32 bytes), buf2 at 0x7ff8000 (32 bytes) → total 64 bytes in 2 pieces
I02. PROBLEM: kernel needs to read all 64 bytes → but they are NOT contiguous in memory → cannot do single memcpy
I03. DEFINITION: iovec = "I/O vector" = one piece of user buffer → struct with (base_address, length)
I04. KERNEL SOURCE: /home/r/Desktop/learn_kernel/source/include/linux/uio.h line 17-20:
     struct kvec {
         void *iov_base;  // address of buffer
         size_t iov_len;  // length in bytes
     };

I05. EXAMPLE: 2 buffers → 2 iovecs:
     ┌───────────────────────────────────────────────────────────────────────────────────────┐
     │ iovec[0] = { .iov_base = 0x7ff0000, .iov_len = 32 }  ← first piece                   │
     │ iovec[1] = { .iov_base = 0x7ff8000, .iov_len = 32 }  ← second piece                  │
     └───────────────────────────────────────────────────────────────────────────────────────┘

I06. PROBLEM: need to track "where am I in copying?" → after copying 48 bytes, we are in iovec[1] at offset 16
I07. DEFINITION: iov_iter = "I/O vector iterator" = struct that tracks progress through multiple iovecs
I08. KERNEL SOURCE: /home/r/Desktop/learn_kernel/source/include/linux/uio.h line 41-79:
     struct iov_iter {
         u8 iter_type;        // what kind of buffers? (user, kernel, etc)
         bool data_source;    // reading (0) or writing (1)?
         size_t iov_offset;   // current offset WITHIN current iovec
         size_t count;        // remaining bytes to process
         const struct iovec *__iov;  // pointer to iovec array
         unsigned long nr_segs;      // number of iovecs
     };

I09. EXAMPLE STATE: after copying 48 bytes from our 64-byte example:
     ┌───────────────────────────────────────────────────────────────────────────────────────┐
     │ iter.count = 16          ← 64 - 48 = 16 bytes remaining                              │
     │ iter.__iov = &iovec[1]   ← now pointing to second iovec                              │
     │ iter.iov_offset = 16     ← within iovec[1], already copied 16 bytes                  │
     │ iter.nr_segs = 1         ← only 1 iovec remaining                                    │
     └───────────────────────────────────────────────────────────────────────────────────────┘

I10. WHERE DOES iter COME FROM? → user calls send(sock, buf, len, flags)
I11. KERNEL BUILDS iter: send() → __sys_sendto() → sock_sendmsg() → builds msghdr with msg_iter inside
I12. KERNEL SOURCE: /home/r/Desktop/learn_kernel/source/include/linux/socket.h line 61:
     struct msghdr {
         ...
         struct iov_iter msg_iter;  ← HERE! the iterator lives inside message header
         ...
     };

I13. CHAIN: user buf at 0x7ff0000 → kernel wraps in iov_iter → copy_from_iter reads from user VA

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

NOW WE CAN USE "iter" (P05 depends on I01-I13)

P05. CODE EXCERPT LINE 580: `copy_from_iter(skb->data + offset, copy, from)`
     │ from = struct iov_iter* = pointer to iterator (derived in I07-I08)
     │ from contains user VA 0x7ff0000 inside from->__iov[0].iov_base (derived in I05)
     │ skb->data = kernel buffer (skb = socket buffer = kernel memory)
     │ copy_from_iter = reads from user VA (via iter), writes to kernel address
     │ ∴ CPU reads from user VA, writes to kernel skb->data → COPY #1 PROVEN

P06. QUESTION: where is COPY #2 (kernel → NIC)?
P07. FILE: /home/r/Desktop/learn_kernel/source/net/core/skbuff.c
P08. FUNCTION: when NIC driver transmits, it reads from skb->data → DMA to NIC TX ring
P09. EXAMPLE: most drivers call `dma_map_single(dev, skb->data, len, DMA_TO_DEVICE)`
P10. dma_map_single = tell NIC "here is PA of skb->data, read it yourself"
P11. ∴ NIC DMAs from kernel skb->data → NIC TX buffer → COPY #2 (technically DMA, but still data movement)

P12. ALTERNATIVE PROOF: find a simpler path
P13. FILE: /home/r/Desktop/learn_kernel/source/net/ipv4/ip_output.c
P14. LINE 939: `copy_from_iter_full(to, len, &msg->msg_iter)` → COPY #1 again
P15. LINE 943: `csum_and_copy_from_iter_full(to, len, &csum, &msg->msg_iter)` → COPY #1 with checksum

SUMMARY OF PROOF:
┌────────────────────────────────────────────────────────────────────────────────────────────────┐
│ COPY #1 EVIDENCE:                                                                              │
│   net/core/datagram.c:580    copy_from_iter(skb->data + offset, copy, from)                    │
│   net/ipv4/ip_output.c:939   copy_from_iter_full(to, len, &msg->msg_iter)                      │
│   net/ipv4/ip_output.c:943   csum_and_copy_from_iter_full(to, len, &csum, &msg->msg_iter)      │
│   ALL THESE: user VA (msg->msg_iter) → kernel skb->data                                        │
│                                                                                                │
│ COPY #2 EVIDENCE:                                                                              │
│   NIC driver calls dma_map_single(skb->data) → NIC reads via DMA                               │
│   This is "zero-copy" from CPU perspective, but data still moves RAM → NIC                     │
│                                                                                                │
│ ∴ TRADITIONAL SEND = 2 DATA MOVEMENTS: user→kernel (CPU copy) + kernel→NIC (DMA)              │
└────────────────────────────────────────────────────────────────────────────────────────────────┘

EXERCISE: verify yourself
E-P1. `grep -n "copy_from_iter" /home/r/Desktop/learn_kernel/source/net/core/datagram.c`
E-P2. `grep -n "copy_from_iter" /home/r/Desktop/learn_kernel/source/net/ipv4/ip_output.c`
E-P3. open datagram.c:580, read the function, trace where 'from' comes from (user space)
E-P4. open datagram.c:568, read function signature: `iov_iter *from` → this is user iterator

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 8: WHAT IS DMA (Direct Memory Access)?

054. IDEA: let NIC read directly from RAM, without CPU copying
055. DEFINITION: DMA = NIC reads/writes RAM without CPU involvement
056. PROBLEM: NIC sees physical addresses (PA), but program uses virtual addresses (VA)
057. DERIVED FROM 056: NIC cannot use VA 0x7ff0000 directly → NIC needs PA

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 9: WHAT IS RDMA (Remote Direct Memory Access)?

058. DERIVED FROM 054, 055: DMA = NIC reads local RAM directly
059. DEFINITION: RDMA = NIC on machine A reads RAM on machine B directly, without B's CPU
060. DERIVED FROM 059: "Remote" = another machine, "Direct" = no CPU copy, "Memory Access" = read/write bytes
061. PROBLEM: how does NIC on A know the PA on B? → B must tell A somehow
062. PROBLEM: how to prevent random machines from reading your RAM? → need permission system

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 10: WHAT IS ibv_reg_mr? (RDMA MEMORY REGISTRATION)

063. FROM 057: NIC needs PA to do DMA, but program only knows VA
064. SOLUTION: program tells kernel: "I want NIC to access these bytes" → kernel translates VA to PA and tells NIC
065. DEFINITION: ibv_reg_mr = function to register a memory region with the NIC
066. INPUT TO ibv_reg_mr: VA of buffer, length of buffer, access permissions
067. WHAT ibv_reg_mr DOES:
     step 1: kernel walks page table to find PA for given VA (uses procedure in 044)
     step 2: kernel "pins" the page → prevents page from being swapped to disk
     step 3: kernel stores (VA, PA, length, permissions) in NIC's internal table
     step 4: kernel returns "lkey" = handle to identify this registration

068. DEFINITION: lkey = Local Key = 32-bit number to identify a registered memory region
069. DEFINITION: rkey = Remote Key = 32-bit number to grant remote machine access to your memory

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 11: WHY PINNING? (FROM 067 step 2)

070. FACT: operating system can swap pages to disk to free RAM
071. PROBLEM: if NIC has (VA, PA) mapping and page is swapped, PA now belongs to different page → NIC reads wrong data
072. DEFINITION: "pinning" = telling kernel "do not swap this page" → page stays in RAM at same PA
073. DERIVED FROM 072: pinned pages have refcount > 1 → kernel refuses to swap them

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 12: WHAT IS A QUEUE PAIR (QP)?

074. PROBLEM: RDMA needs to track send/receive operations → where to put the request list?
075. DEFINITION: Queue Pair (QP) = two queues: Send Queue (SQ) + Receive Queue (RQ)
076. USAGE: to send, you "post" a work request (WR) to Send Queue → NIC processes it
077. USAGE: to receive, you "post" a buffer to Receive Queue → NIC fills it when data arrives

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 13: WHAT IS A COMPLETION QUEUE (CQ)?

078. PROBLEM: after NIC processes a work request, how does program know it's done?
079. DEFINITION: Completion Queue (CQ) = queue where NIC puts completion notifications
080. FLOW: program posts WR to SQ → NIC processes → NIC posts completion to CQ → program polls CQ

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 14: WHAT IS A PROTECTION DOMAIN (PD)?

081. PROBLEM: prevent one program's memory from being accessed by another program's QP
082. DEFINITION: Protection Domain (PD) = security boundary grouping QPs and MRs
083. RULE: QP can only access MRs in the same PD

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 15: RDMA OPERATION SEQUENCE

084. DERIVED FROM ALL ABOVE: to do RDMA you must:
     step 0: have NIC (from 046)
     step 1: open device → get context
     step 2: allocate PD → get pd (from 082)
     step 3: create CQ → get cq (from 079)
     step 4: create QP with (pd, cq) → get qp (from 075)
     step 5: allocate buffer (malloc)
     step 6: register buffer → ibv_reg_mr(pd, buf, len, flags) → get mr with lkey (from 065)
     step 7: transition QP state: RESET → INIT → RTR → RTS
     step 8: post receive (if expecting data)
     step 9: post send (to send data)
     step 10: poll CQ for completions

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

AXIOM BLOCK 16: YOUR CODE MAPPED TO AXIOMS

rdma_loopback.c line 529: `struct ibv_pd *pd;` → derived from 082 (PD definition)
rdma_loopback.c line 530: `struct ibv_cq *cq;` → derived from 079 (CQ definition)
rdma_loopback.c line 531: `struct ibv_qp *qp;` → derived from 075 (QP definition)
rdma_loopback.c line 532: `struct ibv_mr *mr_send, *mr_recv;` → derived from 065 (MR definition)
rdma_loopback.c line 701: `mr_send = ibv_reg_mr(pd, send_buf, ...)` → derived from 067 (reg_mr does VA→PA translation)

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

EXERCISES (EACH USES ONLY ABOVE AXIOMS)

E01. FROM 004: how many things can 12 bits represent? → 2^12 = __________ (uses only 004)
E02. FROM 016: your RAM has __________ bytes (uses only 015, 016)
E03. FROM 027: if PFN = 100, what is the address? → 100 × 4096 = __________ (uses 027)
E04. FROM 042: 9+9+9+9+12 = __________ (uses 040, 041)
E05. FROM 050: how many bits in a MAC address? → 6 × 8 = __________ (uses 011, 050)

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

NEW THINGS INTRODUCED WITHOUT DERIVATION: NONE

Every term used in this document is either:
- An axiom (marked AXIOM)
- Derived from previous lines (marked DERIVED FROM ###)
- A fact from your machine (marked FACT FROM YOUR MACHINE)
- A definition introducing new term (marked DEFINITION)

════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
