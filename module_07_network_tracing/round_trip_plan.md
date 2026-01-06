# FULL ROUND-TRIP NETWORK COPY TRACING — TASK GRILLING DOCUMENT — NO SOLUTIONS

## OBJECTIVE STATEMENT

Trace EVERY copy when data travels:
1. User (sender) → Kernel → NIC TX
2. NIC RX → Kernel → User (receiver)

Same string "HELLO_SEND_TRACE" tracked through all 4 copies.

## USER DECISIONS (2026-01-06)

- MODULES: 2 separate modules
  - send_trace_hw.c: traces COPY #1 (user→kernel) and COPY #2 (kernel→NIC)
  - recv_trace_hw.c: traces COPY #3 (NIC→kernel) and COPY #4 (kernel→user)
- INTERFACE: Real NIC only, NO loopback
  - Device: wlp3s0
  - IP: 192.168.29.158
  - Need TWO machines OR send to router and back? (need clarification)
- TIMEOUT: 90 seconds for receiver (SO_RCVTIMEO)
- DATA FLOW: userspaceA → driverA → [wire] → driverB → userspaceB

## QUESTION: How to test real NIC with one machine?

Q-NEW-1: For real NIC copy, need packet to actually go out → come back
Q-NEW-2: Options:
  a) Two machines on same network (192.168.29.x)
  b) Send to router (192.168.29.1) and expect ICMP/reply?
  c) Use raw socket to craft packet that triggers response?
  d) Send UDP to self (192.168.29.158) — does this bypass loopback?

VERIFY: Does sending to own IP (192.168.29.158) use wlp3s0 or loopback?
RESULT: `ip route get 192.168.29.158` → `local 192.168.29.158 dev lo` → USES LOOPBACK, NOT wlp3s0!

∴ FOR REAL NIC TRACING, NEED EXTERNAL TARGET:
- Option A: Another machine on 192.168.29.x (run receiver there)
- Option B: Send to router 192.168.29.1 (but router won't echo UDP back)
- Option C: Phone on same WiFi (run netcat as receiver)
- Option D: VM with bridged networking (separate IP like 192.168.29.159)

USER MUST DECIDE: What is the target IP for real NIC test?

## COPY MAP (TO BE PROVEN)

```
SENDER MACHINE                                    RECEIVER MACHINE (same for loopback)
┌─────────────────┐                              ┌─────────────────┐
│ sender.c        │                              │ receiver.c      │
│ buf="HELLO..."  │                              │ buf=???         │
│ VA=0x649521f61069│                              │ VA=???          │
└────────┬────────┘                              └────────▲────────┘
         │ COPY #1: copy_from_user                        │ COPY #4: copy_to_user
         ▼                                                │
┌─────────────────┐                              ┌─────────────────┐
│ Kernel skb (TX) │                              │ Kernel skb (RX) │
│ skb->data=0xffff│                              │ skb->data=???   │
└────────┬────────┘                              └────────▲────────┘
         │ COPY #2: to NIC TX buffer                      │ COPY #3: from NIC RX buffer
         ▼                                                │
┌─────────────────────────────────────────────────────────┐
│                NIC / LOOPBACK DEVICE                    │
│  For lo: packet goes directly TX queue → RX queue      │
│  For real NIC: DMA to wire → DMA from wire             │
└─────────────────────────────────────────────────────────┘
```

---

## TASK 0: WHAT RECEIVER PROGRAM DO WE NEED?

### Grilling Questions (no solutions)

Q0.1: receiver.c calls recv() or recvfrom()? Which one for UDP?
Q0.2: Does receiver need to bind() to port 9999 before recv()?
Q0.3: Does receiver need to call listen()? (TCP yes, UDP no?)
Q0.4: How does receiver know when data arrives? Blocking recv()?
Q0.5: Where does receiver's buffer live? Stack? Heap? What VA?
Q0.6: How to ensure receiver is running BEFORE sender sends?
Q0.7: What if receiver buffer is smaller than message? Truncation?
Q0.8: How to print receiver's buffer VA for correlation with kprobe?

### Sub-tasks (no solutions)

T0.1: Write receiver.c with socket(), bind(), recvfrom()
T0.2: Receiver prints its buffer VA before recv
T0.3: Receiver prints received data after recv
T0.4: Start receiver in one terminal, sender in another
T0.5: Verify data arrives correctly before adding kprobes

---

## TASK 1: WHAT KERNEL FUNCTION DOES COPY #3? (NIC → Kernel skb)

### Grilling Questions (no solutions)

Q1.1: When packet arrives at NIC, what triggers kernel?
Q1.2: Is it interrupt? Softirq? NAPI poll?
Q1.3: What function allocates skb for received packet?
Q1.4: For loopback, is there a "receive"? Or same skb reused?
Q1.5: What is __netif_receive_skb? Is it the entry point?
Q1.6: What is ip_rcv? udp_rcv? Where in the chain?
Q1.7: Is there a copy from NIC buffer to skb, or DMA directly to skb?
Q1.8: For loopback, skb->data is already kernel memory, so no COPY #3?

### Counter-questions

C1.1: If loopback reuses TX skb for RX, then COPY #2 and #3 are ZERO copies?
C1.2: For real NIC, where does DMA write? To skb->data? To ring buffer?
C1.3: If DMA writes directly to skb->data, is it still called a "copy"?

---

## TASK 2: WHAT KERNEL FUNCTION DOES COPY #4? (Kernel skb → User)

### Grilling Questions (no solutions)

Q2.1: User calls recv(fd, buf, len, flags) — what syscall number?
Q2.2: What kernel function handles recvfrom syscall?
Q2.3: Where is copy_to_user or copy_to_iter called?
Q2.4: Is it in udp_recvmsg()? Or deeper?
Q2.5: What is skb_copy_datagram_iter? Is it the copy function?
Q2.6: Is skb_copy_datagram_msg different from skb_copy_datagram_iter?
Q2.7: How to extract destination VA (user buffer) from kprobe?
Q2.8: How to match this copy to the specific recv() call?

### Sub-tasks (no solutions)

T2.1: Find recvfrom syscall entry in kernel source
T2.2: Trace: __sys_recvfrom → sock_recvmsg → udp_recvmsg → ???
T2.3: Find where copy_to_iter or copy_to_user is called
T2.4: Add kprobe on that function
T2.5: Capture: source (skb->data), dest (user VA), length

### Counter-questions

C2.1: Is copy_to_iter the inverse of copy_from_iter?
C2.2: Does copy_to_iter internally call raw_copy_to_user?
C2.3: Can we reuse same kprobe handler pattern from COPY #1?

---

## TASK 3: WHAT KERNEL FUNCTION DOES COPY #2? (Kernel skb → NIC TX)

### Grilling Questions (no solutions)

Q3.1: We already probed __dev_queue_xmit for COPY #2, correct?
Q3.2: But __dev_queue_xmit is just the entry, where is actual copy?
Q3.3: For loopback, is there ANY copy? Or just pointer passing?
Q3.4: For real NIC, where is dma_map_single called?
Q3.5: What is the difference between __dev_queue_xmit and dev_hard_start_xmit?
Q3.6: Is xmit_one the function that calls driver's ndo_start_xmit?
Q3.7: For loopback driver, what is ndo_start_xmit? loopback_xmit?
Q3.8: Does loopback_xmit call netif_rx to put packet in RX queue?

### Sub-tasks (no solutions)

T3.1: Find loopback driver source (drivers/net/loopback.c)
T3.2: Find loopback_xmit function
T3.3: Trace what loopback_xmit does with skb
T3.4: Determine: is there a copy or just skb pointer moved?

### Counter-questions

C3.1: If loopback just moves skb from TX to RX, is COPY #2 zero?
C3.2: Then for loopback, only COPY #1 and COPY #4 are real copies?
C3.3: Does this mean loopback is 2-copy, not 4-copy?

---

## TASK 4: SEPARATE DRIVER MODULE FOR RECEIVE PATH

### Grilling Questions (no solutions)

Q4.1: Should we add receive kprobes to existing send_trace_hw.c?
Q4.2: Or create new recv_trace_hw.c for receive path?
Q4.3: What functions to probe for receive path?
Q4.4: How to filter only our receiver process (by PID? by comm?)?
Q4.5: Receive path runs in softirq context — can we use printk?
Q4.6: What if packet processed before receiver calls recv()?
Q4.7: How to correlate skb from COPY #3 to COPY #4?

### Sub-tasks (no solutions)

T4.1: Decide: one module or two modules?
T4.2: List candidate functions for kprobes on receive path
T4.3: Test each kprobe individually
T4.4: Combine into single module with 4 kprobes

---

## TASK 5: STRING MATCHING — VERIFY SAME DATA IN ALL 4 COPIES

### Grilling Questions (no solutions)

Q5.1: How to verify "HELLO_SEND_TRACE" is same at each copy point?
Q5.2: Can kprobe handler read skb->data and print first 16 bytes?
Q5.3: Can kprobe handler read user buffer? (access_ok needed?)
Q5.4: What if data is fragmented across multiple skbs?
Q5.5: What if checksum is computed, does data change?
Q5.6: How to print hex dump of data in dmesg?

### Sub-tasks (no solutions)

T5.1: In COPY #1 handler, print first 16 bytes of user buffer
T5.2: In COPY #2 handler, print first 16 bytes of skb->data
T5.3: In COPY #3 handler, print first 16 bytes of skb->data
T5.4: In COPY #4 handler, print first 16 bytes of user buffer
T5.5: Compare all 4 — should be identical "HELLO_SEND_TRACE"

---

## TASK 6: UPDATE WORKSHEET WITH ALL 4 TRACES

### Grilling Questions (no solutions)

Q6.1: Should we have 4 separate pseudo-debugger traces?
Q6.2: Or one combined trace showing all 4 copies in order?
Q6.3: How to show the "handoff" between TX and RX (loopback)?
Q6.4: What timestamps should we capture? ktime_get_ns()?
Q6.5: How to calculate total latency: sender send() → receiver recv()?

---

## PARADOXES TO RESOLVE

P1: For LOOPBACK, how many copies actually happen?
   - Is it 4 (user→skb→TX→RX→skb→user)?
   - Or 2 (user→skb, skb→user) because TX→RX is just pointer move?

P2: If COPY #2 and #3 are zero for loopback, why have two probes?
   - Should we detect "loopback" vs "real NIC" and report differently?

P3: For real NIC with DMA, is DMA a "copy"?
   - CPU doesn't execute memcpy, NIC DMA engine does
   - Does it count as copy? Or as "transfer"?

P4: Multiple skbs for one message?
   - What if message is split across fragments?
   - How to track all fragments?

P5: Receive path async from sender
   - Receiver might call recv() BEFORE or AFTER packet arrives
   - How to ensure we capture in correct order?

---

## FILES TO CREATE

```
/home/r/Desktop/ainv/send_trace/
├── sender.c              (already exists)
├── receiver.c            (NEW: binds to 9999, calls recvfrom, prints buffer VA)
├── send_trace_hw.c       (already exists, has COPY #1 and #2 probes)
├── recv_trace_hw.c       (NEW: probes for COPY #3 and #4? Or extend existing?)
├── Makefile              (update to build receiver and new module)
└── worksheet.md          (update with all 4 copy traces)
```

---

## KERNEL FUNCTIONS TO INVESTIGATE

For COPY #3 (NIC → kernel skb):
- __netif_receive_skb_core
- ip_rcv
- udp_rcv
- __udp4_lib_rcv
- udp_queue_rcv_skb

For COPY #4 (kernel skb → user):
- __sys_recvfrom
- sock_recvmsg
- udp_recvmsg
- skb_copy_datagram_msg
- skb_copy_datagram_iter
- copy_to_iter
- _copy_to_iter
- raw_copy_to_user

---

## OPEN QUESTIONS FOR USER

1. For COPY #3: Do you want to see NIC→kernel even if loopback has no real copy?
2. Should receiver.c be blocking (waits for packet) or timeout?
3. One kernel module with 4 probes, or two modules (send + recv)?
4. Do you want timing info (nanoseconds between copies)?
5. Do you want hex dump of data at each copy point?

---

## DEPENDENCY GRAPH

```
                    receiver.c (bind to 9999)
                            │
                            ▼
                    Start receiver first
                            │
            ┌───────────────┴───────────────┐
            ▼                               ▼
     sender.c sends              recv_trace_hw.c loaded
            │                               │
            ▼                               ▼
     send_trace_hw.c probes      kprobes on recv path
     COPY #1, COPY #2            COPY #3, COPY #4
            │                               │
            └───────────────┬───────────────┘
                            ▼
                    Combined dmesg output
                            ▼
                    Worksheet with all 4 traces
```

---

## FAILURE PREDICTIONS

F1: receiver.c doesn't bind correctly → EADDRINUSE if port in use
F2: receiver starts after sender → packet lost, no COPY #3/#4
F3: kprobe on udp_recvmsg fails → function inlined? Try __sys_recvfrom
F4: COPY #3 kprobe never fires → probe on wrong receive function?
F5: Different PIDs for sender/receiver → filter both in kprobe handlers
F6: Loopback has no COPY #2 or #3 → expected for loopback, not an error

---

## NO SOLUTIONS WRITTEN — ALL GRILLING ONLY

Questions: 56 (Q)
Sub-tasks: 26 (T)
Counter-questions: 10 (C)
Paradoxes: 5 (P)
Failure predictions: 6 (F)

User must answer questions before we write code.
