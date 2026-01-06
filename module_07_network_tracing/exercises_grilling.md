# NETWORK COPY TRACE EXERCISES — TASK GRILLING — NO SOLUTIONS — REAL MACHINE DATA

## MACHINE STATE (CAPTURED 2026-01-06)

Sender PID: 40764, Receiver PID: 40761
Sender buf VA: 0x6130204f0069
Receiver buf VA: 0x7ffe943c11d0
Kernel skb dest: 0xffff8cb08de63c00
Kernel skb COPY2: 0xffff8cb00df8acfe

---

## GRILLING BLOCK 0: PHYSICAL ADDRESS CALCULATION

Q0.1: You saw recv_buf PA: 0x1d0 — this is WRONG. Why?
Q0.2: pagemap needs root to read PFN. You ran without root. What did you get?
Q0.3: 0x1d0 = page offset only (0x7ffe943c11d0 & 0xFFF). Calculate yourself:
      0x7ffe943c11d0 & 0xFFF = ???
      FILL: _____________
Q0.4: If PFN were 0x12340, what would full PA be?
      PA = (PFN << 12) | page_offset = (0x12340 << 12) | 0x1d0 = ???
      FILL: _____________
Q0.5: Did pagemap return PFN = 0? What does bit 63 = 0 mean?
Q0.6: How to fix? Run with `sudo ./receiver`? Or use CAP_SYS_ADMIN?

COUNTER-QUESTION: Why does /proc/self/pagemap hide PFN from non-root?
PARADOX: If you can read your own memory (VA), why can't you see PA?

---

## GRILLING BLOCK 1: KERNEL VA → PA CALCULATION

Q1.1: Kernel skb->data = 0xffff8cb08de63c00. What is PA?
Q1.2: Kernel direct map starts at 0xffff888000000000 on your machine. VERIFY:
      cat /proc/kallsyms | grep " page_offset_base"
      RESULT: _____________
Q1.3: If page_offset_base = 0xffff888000000000, then:
      PA = VA - page_offset_base
      PA = 0xffff8cb08de63c00 - 0xffff888000000000 = ???
      FILL: _____________
Q1.4: But wait — your VA starts with 0xffff8cb0, not 0xffff8880. Is formula wrong?
Q1.5: Check KASLR: cat /proc/cmdline | grep kaslr
      RESULT: _____________
Q1.6: KASLR randomizes kernel base. YOUR machine's actual page_offset_base is shifted.
      How to find REAL offset? sudo cat /proc/kallsyms | grep page_offset_base
      RESULT: _____________

COUNTER-QUESTION: If KASLR shifts kernel VA, how does kernel know where RAM is?
PARADOX: VA is randomized but PA (RAM) is fixed at boot. Mismatch?

---

## GRILLING BLOCK 2: WHERE IS skb->data IN STRUCT?

Q2.1: You probed _copy_from_iter with dest=0xffff8cb08de63c00. This is skb->data?
Q2.2: Look at struct sk_buff in kernel source. Where is 'data' field?
      grep -n "unsigned char.*\*data" /home/r/Desktop/learn_kernel/source/include/linux/skbuff.h
      LINE NUMBER: _____________
Q2.3: sizeof(struct sk_buff) is ~232 bytes (varies by config). If skb struct is at 0xffff8cb08de63b00, where is skb->data pointer?
      offset of 'data' in sk_buff = ??? (need to count fields)
Q2.4: The pointer skb->data POINTS TO buffer. skb->data itself is stored in skb struct. But the VALUE in skb->data points elsewhere. Clarify this.
Q2.5: Your kprobe captured dest=0xffff8cb08de63c00. Is this:
      a) Address of skb->data field in struct?
      b) Value of skb->data (where payload goes)?
      ANSWER: _____________

COUNTER-QUESTION: kprobe gives regs->di = first argument. For _copy_from_iter(dest, len, iter), dest = where copy WRITES to. So it's (b).
PARADOX: You captured 3 COPY1 entries with same dest but len=22, 1, 37. Why 3 calls?

---

## GRILLING BLOCK 3: WHY len=22, 1, 37 INSTEAD OF len=16?

Real dmesg:
[COPY1] PID=40764 comm=sender dest=ffff8cb08de63c00 len=22
[COPY1] PID=40764 comm=sender dest=ffff8cb08de63c00 len=1
[COPY1] PID=40764 comm=sender dest=ffff8cb08de63c00 len=37

Q3.1: You sent 16 bytes "HELLO_SEND_TRACE". But kernel copied 22 + 1 + 37 = 60 bytes. WHY?
Q3.2: UDP packet has: 8 byte UDP header + payload. But 8 + 16 = 24, not 60.
Q3.3: IP header is 20 bytes. 20 + 8 + 16 = 44, not 60.
Q3.4: Ethernet header is 14 bytes. 14 + 20 + 8 + 16 = 58. Still not 60.
Q3.5: Are you probing MORE than just your payload copy?
Q3.6: _copy_from_iter is called EVERYWHERE, not just for your send(). Other kernel code uses it.
      How to FILTER only your 16-byte payload copy?

COUNTER-QUESTION: Your filter is `strncmp(current->comm, "sender", 6)`. But "sender" PID might have other copies happening (DNS, systemd, etc)?
PARADOX: Same dest address 0xffff8cb08de63c00 for all 3 calls. Is kernel reusing same buffer?

---

## GRILLING BLOCK 4: COPY #4 TRACE — RECEIVE PATH

Q4.1: You have NO kprobe for receive path. recv_trace_hw.c does not exist.
Q4.2: What function does copy_to_user for UDP receive?
      Candidates:
      - skb_copy_datagram_iter
      - skb_copy_datagram_msg
      - copy_to_iter
      - _copy_to_iter
Q4.3: Which is actually called? grep kernel source:
      grep -rn "copy_to_iter" /home/r/Desktop/learn_kernel/source/net/ipv4/udp.c
      RESULT: _____________
Q4.4: Can you probe _copy_to_iter like you probed _copy_from_iter?
      Check if exported: grep "_copy_to_iter" /proc/kallsyms
      RESULT: _____________
Q4.5: Handler needs: source (skb->data), dest (user VA), len
      For _copy_to_iter(src, bytes, iter), which arg is dest?
      FILL: regs->di = ___, regs->si = ___, regs->dx = ___

COUNTER-QUESTION: _copy_to_iter copies TO iter, not FROM iter. iter describes user buffer. How to get user VA from iter?
PARADOX: iter might be iovec, kvec, or ubuf. Which type for recv()?

---

## GRILLING BLOCK 5: LOOPBACK SKB REUSE

dmesg shows:
[COPY2] dev=lo skb_data=ffff8cb00df8acfe skb_len=66

Q5.1: COPY1 dest was 0xffff8cb08de63c00. COPY2 skb_data is 0xffff8cb00df8acfe. DIFFERENT!
Q5.2: Why different? Kernel might allocate NEW skb for transmit?
Q5.3: Or skb->data moves within same skb (skb_push/skb_pull)?
Q5.4: Check loopback driver source:
      cat /home/r/Desktop/learn_kernel/source/drivers/net/loopback.c | head -100
Q5.5: Find loopback_xmit function. Does it clone skb or reuse?
Q5.6: If skb is CLONED, is skb->data same or different?

COUNTER-QUESTION: skb_clone() shares skb->data. skb_copy() allocates new data. Which does loopback use?
PARADOX: If loopback clones, COPY1 dest should equal COPY2 skb_data. But they differ.

---

## GRILLING BLOCK 6: PACKET SIZE MATH

skb_len=66 bytes

Q6.1: Your payload = 16 bytes ("HELLO_SEND_TRACE")
Q6.2: UDP header = 8 bytes → 16 + 8 = 24
Q6.3: IP header = 20 bytes → 24 + 20 = 44
Q6.4: Ethernet header = 14 bytes → 44 + 14 = 58
Q6.5: 66 - 58 = 8 bytes EXTRA. Where from?
Q6.6: Ethernet minimum frame is 64 bytes. 58 < 64. Padding needed?
      Padding = 64 - 58 = 6 bytes? But you have 8 extra.
Q6.7: Or is this FCS (CRC32 = 4 bytes)? 58 + 4 = 62. Still not 66.
Q6.8: VLAN tag? 4 bytes. 58 + 4 = 62. Nope.
Q6.9: Maybe skb_len includes something else. skb_headroom? skb_tailroom?

COUNTER-QUESTION: Does skb_len = just data, or data + headers? What about skb->len vs skb->data_len?
PARADOX: You see 66, math says 58. 8 bytes unaccounted.

---

## GRILLING BLOCK 7: PHYSICAL RAM VERIFICATION

Q7.1: Sender buf PA = ??? (need to run sender with sudo + pagemap)
Q7.2: Kernel skb PA = ??? (calculated from VA - page_offset_base)
Q7.3: Receiver buf PA = ??? (need to run receiver with sudo + pagemap)
Q7.4: All 3 should be DIFFERENT physical addresses. Verify.
Q7.5: Can you dump physical RAM at those addresses? /dev/mem requires root.
      sudo xxd -s PA -l 16 /dev/mem
      But /dev/mem is restricted. Alternative?
Q7.6: Use kernel module to read physical RAM: phys_to_virt(PA)->read

COUNTER-QUESTION: If you prove different PA for each, you prove 3 copies exist in RAM. But kernel might have freed sender's buffer already.
PARADOX: When sender exits, its pages are freed. Is sender buf still in RAM when receiver reads?

---

## SUB-TASKS (NO SOLUTIONS)

T01: Run `sudo cat /proc/kallsyms | grep page_offset_base` → record result
T02: Calculate: 0xffff8cb08de63c00 - page_offset_base = PA
T03: Create recv_trace_hw.c with kprobe on _copy_to_iter
T04: Run sender+receiver with BOTH kprobe modules loaded
T05: Capture COPY #4: source kernel VA, dest user VA, len
T06: Run sender with sudo, verify PA from pagemap
T07: Run receiver with sudo, verify PA from pagemap
T08: Explain why len=22,1,37 instead of 16
T09: Explain why COPY1 dest ≠ COPY2 skb_data
T10: Explain 66 bytes vs expected 58 bytes

---

## FAILURE PREDICTIONS

F01: _copy_to_iter might be inlined → kprobe registration fails
F02: iter type is ITER_UBUF but you read __iov → crash
F03: Running pagemap without root → PFN = 0
F04: KASLR makes page_offset_base different from documentation
F05: Receiver exits before you check dmesg → lose COPY #4 trace
F06: Multiple packets on loopback → cannot identify YOUR packet

---

## QUESTIONS COUNT

Grilling questions: 42
Counter-questions: 7
Paradoxes: 7
Sub-tasks: 10
Failure predictions: 6

TOTAL: 72 items to address before writing receive trace module.

---

NO SOLUTIONS WRITTEN. USER MUST CALCULATE AND FILL BLANKS.
