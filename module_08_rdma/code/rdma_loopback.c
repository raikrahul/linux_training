/*
RDMA LOOPBACK DEMO - SINGLE FUNCTION AT A TIME
MACHINE: Linux x86_64, PAGE_SIZE=4096, HEAP_START≈0x5f7d8790c000
NIC: rxe0 (Soft-RoCE), MAC=70:66:55:b1:9a:8d, IPv4=192.168.29.158,
GID=fe80::7266:55ff:feb1:9a8d
*/

#include <arpa/inet.h>
#include <errno.h>
#include <infiniband/verbs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│ BUFFER_SIZE = 64 bytes                                                      │
│ What: 64 = 2^6 = 0x40, fits in one cache line (64 bytes on x86_64)          │
│ Why: minimum payload to demonstrate transfer, no partial cache line waste   │
│ Where: will be at HEAP_BASE + offset, e.g., 0x5f7d8790c000 + 0x1000 =
0x5f7d8791D000 │ │ When: allocated at runtime by posix_memalign │ │ Without: if
BUFFER_SIZE=0, ibv_reg_mr fails (length=0 invalid)              │ │ Which: 64
chosen because 64/64=1 cache line, 128/64=2, 4096/64=64           │ │ Middle
calc: if BUFFER_SIZE=1024, then 1024/64=16 cache lines needed        │ │ Edge
calc: BUFFER_SIZE=1 → still 1 cache line (wasted 63 bytes)             │ │ Large
calc: BUFFER_SIZE=4096 → 4096/64=64 cache lines = 1 page              │
└─────────────────────────────────────────────────────────────────────────────┘
*/
#define BUFFER_SIZE 64

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│ GRH_SIZE = 40 bytes (Global Routing Header)                                 │
│ What: UD (Unreliable Datagram) prepends 40-byte GRH to every received msg   │
│ Why: GRH contains source GID (16B) + traffic class (1B) + flow label (3B)   │
│      + payload length (2B) + next header (1B) + hop limit (1B) + dest GID
(16B) │ │      = 16+1+3+2+1+1+16 = 40 bytes │ │ Where: recv_buf[0..39] = GRH,
recv_buf[40..103] = actual payload            │ │ When: hardware/driver writes
GRH before payload on receive completion       │ │ Without: if we allocate only
BUFFER_SIZE, we lose first 40 bytes of payload │ │ Which: total recv alloc =
GRH_SIZE + BUFFER_SIZE = 40 + 64 = 104 bytes      │ │ Middle calc: if
payload=1024, recv_buf_size = 40+1024 = 1064 bytes          │ │ Edge calc:
payload=0 → recv_buf_size = 40+0 = 40 bytes (GRH only)           │ │ Large calc:
payload=4096 → 40+4096 = 4136 bytes → 4136/4096 = 1.01 pages    │
└─────────────────────────────────────────────────────────────────────────────┘
*/
#define GRH_SIZE 40

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│ PORT_NUM = 1                                                                │
│ What: 1 = first (and only) port on rxe0 device                              │
│ Why: rxe0 binds to wlp3s0 which has 1 logical port                          │
│ Where: /sys/class/infiniband/rxe0/ports/1/                                  │
│ When: specified in ibv_query_port(ctx, PORT_NUM, &attr)                     │
│ Without: PORT_NUM=0 → error (ports are 1-indexed in verbs API)              │
│ Which: Mellanox ConnectX-6 has 2 ports → PORT_NUM ∈ {1,2}                   │
│ Calc: if device has N ports, valid range = [1, N]                           │
│       rxe0 has 1 port → valid = {1}, invalid = {0, 2, 3, ...}               │
└─────────────────────────────────────────────────────────────────────────────┘
*/
#define PORT_NUM 1

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│ UD_QKEY = 0x11111111                                                        │
│ What: 32-bit Queue Key = 0x11111111 = 286331153 decimal                     │
│ Why: UD QPs use QKEY to authenticate senders                                │
│      sender must know receiver's QKEY to send; mismatch → packet dropped    │
│ Where: set in ibv_modify_qp(..., IBV_QP_QKEY)                               │
│ When: during QP transition RESET→INIT                                       │
│ Without: QKEY=0 is valid but exposes to any sender with QKEY=0              │
│ Which: any 32-bit value works; 0x11111111 chosen as memorable pattern       │
│ Calc: 0x11111111 binary = 0001 0001 0001 0001 0001 0001 0001 0001           │
│       = (1<<28) + (1<<24) + (1<<20) + (1<<16) + (1<<12) + (1<<8) + (1<<4) + 1│
│       = 268435456 + 16777216 + 1048576 + 65536 + 4096 + 256 + 16 + 1        │
│       = 286331153                                                           │
└─────────────────────────────────────────────────────────────────────────────┘
*/
#define UD_QKEY 0x11111111

/* Global context (set in main, used by helper functions) */
static struct ibv_context *ctx;
static struct ibv_port_attr port_attr;

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│ FUNCTION: create_ud_qp                                                      │
│ What: allocates a Queue Pair of type Unreliable Datagram (UD)               │
│ Why: UD QPs do not require connection setup → simpler loopback              │
│ Where: QP struct lives in kernel; userspace gets handle (pointer)           │
│                                                                             │
│ INPUT: pd = Protection Domain pointer (e.g., 0x5f7d8791a000)                │
│        cq = Completion Queue pointer (e.g., 0x5f7d8791b000)                 │
│                                                                             │
│ struct ibv_qp_init_attr layout (192 bytes on x86_64):                       │
│ ┌────────────────────────────────────────────────────────────────┐          │
│ │ Offset │ Field           │ Size │ Value in this code           │          │
│ ├────────┼─────────────────┼──────┼──────────────────────────────┤          │
│ │ 0x00   │ qp_context      │ 8    │ NULL (0x0)                   │          │
│ │ 0x08   │ send_cq         │ 8    │ cq pointer (0x5f7d8791b000)  │          │
│ │ 0x10   │ recv_cq         │ 8    │ cq pointer (same)            │          │
│ │ 0x18   │ srq             │ 8    │ NULL (0x0, not using SRQ)    │          │
│ │ 0x20   │ cap.max_send_wr │ 4    │ 10                           │          │
│ │ 0x24   │ cap.max_recv_wr │ 4    │ 10                           │          │
│ │ 0x28   │ cap.max_send_sge│ 4    │ 1                            │          │
│ │ 0x2C   │ cap.max_recv_sge│ 4    │ 1                            │          │
│ │ 0x30   │ cap.max_inline  │ 4    │ 0 (default)                  │          │
│ │ 0x34   │ qp_type         │ 4    │ IBV_QPT_UD = 4               │          │
│ │ 0x38   │ sq_sig_all      │ 4    │ 0 (only signaled sends)      │          │
│ └────────┴─────────────────┴──────┴──────────────────────────────┘          │
│                                                                             │
│ OUTPUT: struct ibv_qp* (e.g., 0x5f7d8791c000)                               │
│ struct ibv_qp layout (first 64 bytes):                                      │
│ ┌────────────────────────────────────────────────────────────────┐          │
│ │ Offset │ Field   │ Size │ Example Value                        │          │
│ ├────────┼─────────┼──────┼──────────────────────────────────────┤          │
│ │ 0x00   │ context │ 8    │ 0x5f7d87918000 (device context)      │          │
│ │ 0x08   │ qp_num  │ 4    │ 0x13 = 19 decimal                    │          │
│ │ 0x0C   │ padding │ 4    │ (alignment)                          │          │
│ │ 0x10   │ pd      │ 8    │ 0x5f7d8791a000                       │          │
│ │ ...    │ ...     │ ...  │ ...                                  │          │
│ └────────┴─────────┴──────┴──────────────────────────────────────┘          │
│                                                                             │
│ Calc: max_send_wr=10, max_recv_wr=10 → max 10 outstanding sends, 10 recvs   │
│       if we post 11th send before polling, ibv_post_send returns ENOMEM     │
│       each WR ≈ 64 bytes in HW queue → 10*64 = 640 bytes send queue         │
│ Edge: max_send_wr=0 → cannot send anything → useless QP                     │
│ Large: max_send_wr=16384 (typical max) → 16384*64 = 1MB send queue           │
└─────────────────────────────────────────────────────────────────────────────┘
*/
static struct ibv_qp *create_ud_qp(struct ibv_pd *pd, struct ibv_cq *cq) {
  /*
  STEP 1: zero-initialize attr struct (192 bytes → 192/8 = 24 qwords)
  What: memset(&attr, 0, 192) → writes 0x00 to bytes [&attr, &attr+192)
  Why: uninitialized fields could have garbage → undefined behavior
  */
  struct ibv_qp_init_attr attr;
  memset(&attr, 0, sizeof(attr));
  /* sizeof(attr) on x86_64 = 192 bytes = 0xC0 */

  /*
  STEP 2: set send_cq and recv_cq
  What: attr.send_cq = cq → write 8-byte pointer at attr+0x08
        attr.recv_cq = cq → write 8-byte pointer at attr+0x10
  Why: completions for send/recv WRs go to this CQ
  Calc: if cq = 0x5f7d8791b000, then:
        *(uint64_t*)(attr+0x08) = 0x5f7d8791b000
        *(uint64_t*)(attr+0x10) = 0x5f7d8791b000
  */
  attr.send_cq = cq;
  attr.recv_cq = cq;

  /*
  STEP 3: set capabilities
  What: max_send_wr=10 → *(uint32_t*)(attr+0x20) = 0x0000000A
        max_recv_wr=10 → *(uint32_t*)(attr+0x24) = 0x0000000A
        max_send_sge=1 → *(uint32_t*)(attr+0x28) = 0x00000001
        max_recv_sge=1 → *(uint32_t*)(attr+0x2C) = 0x00000001
  Why: max_sge=1 means each WR has exactly 1 scatter-gather entry
       if data is contiguous (it is), 1 SGE is enough
  Calc: if max_send_sge=3 and buffer is 4096 bytes split into 3 parts:
        SGE[0]: addr=buf+0, len=1365; SGE[1]: addr=buf+1365, len=1365; SGE[2]:
  addr=buf+2730, len=1366 1365+1365+1366 = 4096 ✓
  */
  attr.cap.max_send_wr = 10;
  attr.cap.max_recv_wr = 10;
  attr.cap.max_send_sge = 1;
  attr.cap.max_recv_sge = 1;

  /*
  STEP 4: set qp_type = IBV_QPT_UD = 4
  What: *(uint32_t*)(attr+0x34) = 4
  Why: UD (Unreliable Datagram) → no connection, each send specifies destination
  AH Which types exist: IBV_QPT_RC  = 2 (Reliable Connection)   → needs connect
  handshake IBV_QPT_UC  = 3 (Unreliable Connection) → needs connect, no ack
    IBV_QPT_UD  = 4 (Unreliable Datagram)   → no connect, per-send dest
    IBV_QPT_RAW_PACKET = 8                  → raw ethernet
  */
  attr.qp_type = IBV_QPT_UD;

  /*
  STEP 5: call kernel via ioctl
  What: ibv_create_qp(pd, &attr) → eventually calls ioctl(fd, UVERBS_CREATE_QP,
  ...) Where: kernel allocates QP resources, returns handle Output: pointer to
  struct ibv_qp or NULL on error
  */
  return ibv_create_qp(pd, &attr);
}

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│ FUNCTION: modify_ud_qp_to_init                                              │
│ What: transition QP state: RESET → INIT                                     │
│ Why: QP starts in RESET state; must reach RTS to send/recv                  │
│      state machine: RESET → INIT → RTR → RTS                                │
│                                                                             │
│ Required attributes for UD INIT:                                            │
│   qp_state      = IBV_QPS_INIT = 1                                          │
│   pkey_index    = 0 (default partition key)                                 │
│   port_num      = 1 (our only port)                                         │
│   qkey          = 0x11111111 = 286331153                                    │
│                                                                             │
│ struct ibv_qp_attr layout (selected fields):                                │
│ ┌────────────────────────────────────────────────────────────────┐          │
│ │ Offset │ Field      │ Size │ Value                             │          │
│ ├────────┼────────────┼──────┼───────────────────────────────────┤          │
│ │ 0x00   │ qp_state   │ 4    │ 1 (INIT)                          │          │
│ │ 0x28   │ pkey_index │ 2    │ 0                                 │          │
│ │ 0x2A   │ port_num   │ 1    │ 1                                 │          │
│ │ 0x30   │ qkey       │ 4    │ 0x11111111                        │          │
│ └────────┴────────────┴──────┴───────────────────────────────────┘          │
│                                                                             │
│ Mask bits (IBV_QP_*):                                                       │
│   IBV_QP_STATE       = 1 << 0  = 0x00000001                                 │
│   IBV_QP_PKEY_INDEX  = 1 << 4  = 0x00000010                                 │
│   IBV_QP_PORT        = 1 << 5  = 0x00000020                                 │
│   IBV_QP_QKEY        = 1 << 6  = 0x00000040                                 │
│   Combined mask = 0x01 | 0x10 | 0x20 | 0x40 = 0x71 = 113 decimal            │
│                                                                             │
│ OUTPUT: 0 on success, errno on failure                                      │
└─────────────────────────────────────────────────────────────────────────────┘
*/
static int modify_ud_qp_to_init(struct ibv_qp *qp) {
  struct ibv_qp_attr attr;
  memset(&attr, 0, sizeof(attr));
  /* sizeof(attr) = 256 bytes on x86_64 */

  attr.qp_state = IBV_QPS_INIT; /* 1 */
  attr.pkey_index = 0;
  attr.port_num = PORT_NUM; /* 1 */
  attr.qkey = UD_QKEY;      /* 0x11111111 = 286331153 */

  int mask = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_QKEY;
  /* mask = 0x01 | 0x10 | 0x20 | 0x40 = 0x71 */

  return ibv_modify_qp(qp, &attr, mask);
}

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│ FUNCTION: modify_ud_qp_to_rtr                                               │
│ What: transition QP state: INIT → RTR (Ready To Receive)                    │
│ Why: after INIT, QP can receive but not send; RTR enables receiving         │
│                                                                             │
│ For UD QP, only need: qp_state = IBV_QPS_RTR = 2                            │
│ mask = IBV_QP_STATE = 0x01                                                  │
│                                                                             │
│ OUTPUT: 0 on success, errno on failure                                      │
└─────────────────────────────────────────────────────────────────────────────┘
*/
static int modify_ud_qp_to_rtr(struct ibv_qp *qp) {
  struct ibv_qp_attr attr;
  memset(&attr, 0, sizeof(attr));
  attr.qp_state = IBV_QPS_RTR; /* 2 */
  return ibv_modify_qp(qp, &attr, IBV_QP_STATE);
}

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│ FUNCTION: modify_ud_qp_to_rts                                               │
│ What: transition QP state: RTR → RTS (Ready To Send)                        │
│ Why: after RTS, QP can both send and receive                                │
│                                                                             │
│ Required attributes for UD RTS:                                             │
│   qp_state = IBV_QPS_RTS = 3                                                │
│   sq_psn   = 0 (starting Packet Sequence Number for send queue)             │
│                                                                             │
│ mask = IBV_QP_STATE | IBV_QP_SQ_PSN                                         │
│      = 0x01 | 0x10000 = 0x10001 = 65537 decimal                             │
│                                                                             │
│ Calc: PSN is 24-bit → range [0, 2^24-1] = [0, 16777215]                     │
│       after 16777215 sends, PSN wraps to 0                                  │
│       at 10 Gbps, 64-byte packets: 10e9 / (64*8) = 19.5M packets/sec        │
│       wrap time = 16777215 / 19500000 = 0.86 seconds                        │
│                                                                             │
│ OUTPUT: 0 on success, errno on failure                                      │
└─────────────────────────────────────────────────────────────────────────────┘
*/
static int modify_ud_qp_to_rts(struct ibv_qp *qp) {
  struct ibv_qp_attr attr;
  memset(&attr, 0, sizeof(attr));
  attr.qp_state = IBV_QPS_RTS; /* 3 */
  attr.sq_psn = 0;             /* starting sequence number */
  return ibv_modify_qp(qp, &attr, IBV_QP_STATE | IBV_QP_SQ_PSN);
}

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│ FUNCTION: post_ud_receive                                                   │
│ What: post a Receive Work Request to QP's receive queue                     │
│ Why: before sender sends, receiver must have posted a receive buffer        │
│      if no receive posted, incoming packet is dropped (UD is unreliable)    │
│                                                                             │
│ struct ibv_sge (Scatter-Gather Entry) layout (16 bytes):                    │
│ ┌────────────────────────────────────────────────────────────────┐          │
│ │ Offset │ Field  │ Size │ Value                                 │          │
│ ├────────┼────────┼──────┼───────────────────────────────────────┤          │
│ │ 0x00   │ addr   │ 8    │ 0x5f7d8791d000 (recv buffer VA)       │          │
│ │ 0x08   │ length │ 4    │ 104 = GRH(40) + BUFFER(64)            │          │
│ │ 0x0C   │ lkey   │ 4    │ 0x486 (from MR registration)          │          │
│ └────────┴────────┴──────┴───────────────────────────────────────┘          │
│                                                                             │
│ struct ibv_recv_wr (Work Request) layout (48 bytes):                        │
│ ┌────────────────────────────────────────────────────────────────┐          │
│ │ Offset │ Field   │ Size │ Value                                │          │
│ ├────────┼─────────┼──────┼──────────────────────────────────────┤          │
│ │ 0x00   │ wr_id   │ 8    │ 0 (user-defined ID for completion)   │          │
│ │ 0x08   │ next    │ 8    │ NULL (no chained WRs)                │          │
│ │ 0x10   │ sg_list │ 8    │ &sge (pointer to SGE array)          │          │
│ │ 0x18   │ num_sge │ 4    │ 1                                    │          │
│ └────────┴─────────┴──────┴──────────────────────────────────────┘          │
│                                                                             │
│ Calc: if num_sge=3 and total recv = 4096 bytes:                             │
│       sge[0].length + sge[1].length + sge[2].length = 4096                  │
│       driver splits incoming data across 3 memory regions                   │
│ Edge: num_sge=0 → no buffer → what receives the data? → error               │
│ Large: num_sge=16 (typical max) → can scatter 16 chunks                     │
│                                                                             │
│ OUTPUT: 0 on success, errno on failure                                      │
└─────────────────────────────────────────────────────────────────────────────┘
*/
static int post_ud_receive(struct ibv_qp *qp, struct ibv_mr *mr, void *buf,
                           size_t len) {
  /*
  STEP 1: build SGE
  sge.addr = (uintptr_t)buf
    if buf = 0x5f7d8791d000, then sge.addr = 0x5f7d8791d000
  sge.length = len = 104 = 0x68
  sge.lkey = mr->lkey = e.g., 0x486
  */
  struct ibv_sge sge = {
      .addr = (uintptr_t)buf, .length = (uint32_t)len, .lkey = mr->lkey};

  /*
  STEP 2: build recv WR
  wr.wr_id = 0 → when completion arrives, wc.wr_id = 0
  wr.next = NULL → single WR, not a linked list
  wr.sg_list = &sge → pointer to our SGE (on stack)
  wr.num_sge = 1 → one SGE
  */
  struct ibv_recv_wr wr = {
      .wr_id = 0, .next = NULL, .sg_list = &sge, .num_sge = 1};

  struct ibv_recv_wr *bad_wr;
  /*
  STEP 3: call ibv_post_recv
  What: ioctl to kernel → kernel writes WR to QP's recv queue
  If recv queue is full (10 already posted), returns ENOMEM
  */
  return ibv_post_recv(qp, &wr, &bad_wr);
}

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│ FUNCTION: post_ud_send                                                      │
│ What: post a Send Work Request to QP's send queue                           │
│ Why: this initiates data transfer from buf to remote QP                     │
│                                                                             │
│ For UD send, we must specify:                                               │
│   - Address Handle (ah) → contains destination GID/LID                      │
│   - remote_qpn → destination QP number                                      │
│   - remote_qkey → must match receiver's QKEY (0x11111111)                   │
│                                                                             │
│ struct ibv_send_wr layout (selected fields):                                │
│ ┌────────────────────────────────────────────────────────────────┐          │
│ │ Offset │ Field            │ Size │ Value                       │          │
│ ├────────┼──────────────────┼──────┼─────────────────────────────┤          │
│ │ 0x00   │ wr_id            │ 8    │ 0                           │          │
│ │ 0x08   │ next             │ 8    │ NULL                        │          │
│ │ 0x10   │ sg_list          │ 8    │ &sge                        │          │
│ │ 0x18   │ num_sge          │ 4    │ 1                           │          │
│ │ 0x1C   │ opcode           │ 4    │ IBV_WR_SEND = 0             │          │
│ │ 0x20   │ send_flags       │ 4    │ IBV_SEND_SIGNALED = 1       │          │
│ │ 0x50   │ wr.ud.ah         │ 8    │ ah pointer                  │          │
│ │ 0x58   │ wr.ud.remote_qpn │ 4    │ receiver's qp_num           │          │
│ │ 0x5C   │ wr.ud.remote_qkey│ 4    │ 0x11111111                  │          │
│ └────────┴──────────────────┴──────┴─────────────────────────────┘          │
│                                                                             │
│ Calc: IBV_SEND_SIGNALED = 1 → generate CQ entry when send completes         │
│       if not signaled, no completion → cannot know when done                │
│       if all 10 sends unsignaled + 11th send → deadlock (queue full, no way │
│       to drain it because no completions generated)                         │
│ Rule: at least every Nth send must be signaled, where N <= max_send_wr      │
│                                                                             │
│ OUTPUT: 0 on success, errno on failure                                      │
└─────────────────────────────────────────────────────────────────────────────┘
*/
static int post_ud_send(struct ibv_qp *qp, struct ibv_mr *mr, void *buf,
                        size_t len, struct ibv_ah *ah, uint32_t remote_qpn) {
  struct ibv_sge sge = {
      .addr = (uintptr_t)buf, .length = (uint32_t)len, .lkey = mr->lkey};

  struct ibv_send_wr wr;
  memset(&wr, 0, sizeof(wr));
  wr.wr_id = 0;
  wr.next = NULL;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  wr.opcode = IBV_WR_SEND;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.wr.ud.ah = ah;
  wr.wr.ud.remote_qpn = remote_qpn;
  wr.wr.ud.remote_qkey = UD_QKEY;

  struct ibv_send_wr *bad_wr;
  return ibv_post_send(qp, &wr, &bad_wr);
}

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│ FUNCTION: poll_cq                                                           │
│ What: poll Completion Queue until 1 completion arrives                      │
│ Why: after posting send/recv, we must wait for hardware to finish           │
│                                                                             │
│ struct ibv_wc (Work Completion) layout (48 bytes):                          │
│ ┌────────────────────────────────────────────────────────────────┐          │
│ │ Offset │ Field       │ Size │ Example Value                    │          │
│ ├────────┼─────────────┼──────┼──────────────────────────────────┤          │
│ │ 0x00   │ wr_id       │ 8    │ 0 (matches posted WR)            │          │
│ │ 0x08   │ status      │ 4    │ 0 = IBV_WC_SUCCESS               │          │
│ │ 0x0C   │ opcode      │ 4    │ 128 = IBV_WC_RECV                │          │
│ │ 0x10   │ vendor_err  │ 4    │ 0                                │          │
│ │ 0x14   │ byte_len    │ 4    │ 104 (GRH + payload)              │          │
│ │ 0x18   │ imm_data    │ 4    │ 0                                │          │
│ │ 0x1C   │ qp_num      │ 4    │ 0x13                             │          │
│ │ 0x20   │ src_qp      │ 4    │ sender's qp_num                  │          │
│ │ ...    │ ...         │ ...  │ ...                              │          │
│ └────────┴─────────────┴──────┴──────────────────────────────────┘          │
│                                                                             │
│ status values:                                                              │
│   IBV_WC_SUCCESS       = 0  → completed successfully                        │
│   IBV_WC_LOC_LEN_ERR   = 1  → posted buffer too small                       │
│   IBV_WC_LOC_QP_OP_ERR = 2  → QP in error state                             │
│   IBV_WC_REM_ACCESS_ERR= 10 → remote memory not accessible                  │
│   ... (see enum ibv_wc_status)                                              │
│                                                                             │
│ Polling loop:                                                               │
│   ibv_poll_cq(cq, 1, &wc) returns:                                          │
│     0  → no completions yet, try again                                      │
│     1  → 1 completion, wc is filled                                         │
│    <0  → error                                                              │
│                                                                             │
│ Calc: polling 1000000 times at ~100 cycles each = 100M cycles               │
│       at 3 GHz CPU, 100M cycles = 33 ms timeout                             │
│       if network latency > 33 ms, this will falsely timeout                 │
│       For 10 Gbps loopback, latency ≈ 2 µs → 33 ms is plenty                │
│                                                                             │
│ OUTPUT: 0 on success, -1 on failure/timeout                                 │
└─────────────────────────────────────────────────────────────────────────────┘
*/
static int poll_cq(struct ibv_cq *cq) {
  struct ibv_wc wc;
  int polls = 0;
  while (polls < 1000000) {
    int n = ibv_poll_cq(cq, 1, &wc);
    if (n < 0) {
      fprintf(stderr, "poll_cq error: n=%d\n", n);
      return -1;
    }
    if (n > 0) {
      if (wc.status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Completion error: status=%d (%s)\n", wc.status,
                ibv_wc_status_str(wc.status));
        return -1;
      }
      return 0;
    }
    polls++;
  }
  fprintf(stderr, "poll_cq timeout after %d polls\n", polls);
  return -1;
}

/*
┌─────────────────────────────────────────────────────────────────────────────┐
│ FUNCTION: main                                                              │
│ What: entry point; orchestrates device open, QP setup, send/recv, cleanup   │
│ Execution order:                                                            │
│   1. Get device list                                                        │
│   2. Open device context                                                    │
│   3. Query port                                                             │
│   4. Query GID                                                              │
│   5. Allocate PD                                                            │
│   6. Create CQ                                                              │
│   7. Create UD QP                                                           │
│   8. Transition QP: RESET→INIT→RTR→RTS                                      │
│   9. Allocate and register send/recv buffers                                │
│  10. Create Address Handle (for loopback to self)                           │
│  11. Post receive                                                           │
│  12. Post send                                                              │
│  13. Poll send completion                                                   │
│  14. Poll recv completion                                                   │
│  15. Verify data                                                            │
│  16. Cleanup                                                                │
│                                                                             │
│ Memory layout at runtime:                                                   │
│ ┌──────────────────────────────────────────────────────────────────────────┐│
│ │ STACK (grows down from 0x7fff...)                                        ││
│ │ ├─ main() frame                                                          ││
│ │ │  ├─ dev_list: 8 bytes (ptr to heap)                                    ││
│ │ │  ├─ ctx: 8 bytes (ptr to heap)                                         ││
│ │ │  ├─ pd: 8 bytes (ptr to heap)                                          ││
│ │ │  ├─ cq: 8 bytes (ptr to heap)                                          ││
│ │ │  ├─ qp: 8 bytes (ptr to heap)                                          ││
│ │ │  ├─ mr_send: 8 bytes (ptr to heap)                                     ││
│ │ │  ├─ mr_recv: 8 bytes (ptr to heap)                                     ││
│ │ │  ├─ send_buf: 8 bytes (ptr to heap)                                    ││
│ │ │  ├─ recv_buf: 8 bytes (ptr to heap)                                    ││
│ │ │  ├─ ah: 8 bytes (ptr to heap)                                          ││
│ │ │  ├─ gid: 16 bytes (union ibv_gid, on stack)                            ││
│ │ │  └─ ...                                                                ││
│ │                                                                          ││
│ │ HEAP (starts at ~0x5f7d8790c000)                                         ││
│ │ ├─ ibv_device** array: 8 bytes × (num_devices + 1)                       ││
│ │ ├─ ibv_context struct: ~1024 bytes                                       ││
│ │ ├─ ibv_pd struct: ~64 bytes                                              ││
│ │ ├─ ibv_cq struct: ~256 bytes                                             ││
│ │ ├─ ibv_qp struct: ~512 bytes                                             ││
│ │ ├─ send_buf: 4096-aligned, 64 bytes                                      ││
│ │ ├─ recv_buf: 4096-aligned, 104 bytes (GRH+payload)                       ││
│ │ ├─ ibv_mr structs: ~64 bytes each                                        ││
│ │ └─ ibv_ah struct: ~128 bytes                                             ││
│ └──────────────────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────────────┘
*/
int main() {
  struct ibv_device **dev_list;
  struct ibv_device *ib_dev;
  struct ibv_pd *pd;
  struct ibv_cq *cq;
  struct ibv_qp *qp;
  struct ibv_mr *mr_send, *mr_recv;
  struct ibv_ah *ah;
  void *send_buf, *recv_buf;
  int num_devices;
  union ibv_gid gid;
  int ret;

  printf("=== RDMA UD Loopback Demo ===\n\n");

  /* ───────────────────────────────────────────────────────────────────────
     STEP 1: Get device list
     What: ibv_get_device_list(&num) → returns array of ibv_device*
     Where: /sys/class/infiniband/ is scanned
     Output: dev_list[0] = ptr to rxe0, dev_list[1] = NULL (terminator)
     ─────────────────────────────────────────────────────────────────────── */
  printf("[01] Getting device list...\n");
  dev_list = ibv_get_device_list(&num_devices);
  if (!dev_list || num_devices == 0) {
    fprintf(stderr, "No RDMA devices. Run: sudo modprobe rdma_rxe && sudo rdma "
                    "link add rxe0 type rxe netdev wlp3s0\n");
    return 1;
  }
  ib_dev = dev_list[0];
  printf("     Device: %s, num_devices=%d\n", ibv_get_device_name(ib_dev),
         num_devices);

  /* ───────────────────────────────────────────────────────────────────────
     STEP 2: Open device context
     What: ibv_open_device(ib_dev) → opens /dev/infiniband/uverbs0
     Output: ctx = pointer to ibv_context (contains fd, ops table, etc.)
     ─────────────────────────────────────────────────────────────────────── */
  printf("[02] Opening device...\n");
  ctx = ibv_open_device(ib_dev);
  if (!ctx) {
    fprintf(stderr, "Failed to open device\n");
    ibv_free_device_list(dev_list);
    return 1;
  }
  ibv_free_device_list(dev_list);

  /* ───────────────────────────────────────────────────────────────────────
     STEP 3: Query port 1
     What: ibv_query_port(ctx, 1, &port_attr) → fills port_attr
     Output: port_attr.lid = 0 (RoCE doesn't use LID)
             port_attr.state = IBV_PORT_ACTIVE = 4
     ─────────────────────────────────────────────────────────────────────── */
  printf("[03] Querying port %d...\n", PORT_NUM);
  if (ibv_query_port(ctx, PORT_NUM, &port_attr)) {
    fprintf(stderr, "Failed to query port\n");
    goto cleanup_ctx;
  }
  printf("     LID=%u, state=%d (4=ACTIVE)\n", port_attr.lid, port_attr.state);

  /* ───────────────────────────────────────────────────────────────────────
     STEP 4: Query GID
     What: ibv_query_gid(ctx, port, index, &gid) → fills gid
     GID for rxe0: fe80:0000:0000:0000:7266:55ff:feb1:9a8d
     gid.raw[0..15] = {0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x72, 0x66, 0x55, 0xff, 0xfe, 0xb1, 0x9a, 0x8d}
     This is derived from MAC 70:66:55:b1:9a:8d:
       Insert ff:fe in middle → 70:66:55:ff:fe:b1:9a:8d
       Flip 7th bit of first byte: 70 = 0111 0000 → 72 = 0111 0010
     ─────────────────────────────────────────────────────────────────────── */
  printf("[04] Querying GID...\n");
  if (ibv_query_gid(ctx, PORT_NUM, 0, &gid)) {
    fprintf(stderr, "Failed to query GID\n");
    goto cleanup_ctx;
  }
  printf("     GID: "
         "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%"
         "02x\n",
         gid.raw[0], gid.raw[1], gid.raw[2], gid.raw[3], gid.raw[4], gid.raw[5],
         gid.raw[6], gid.raw[7], gid.raw[8], gid.raw[9], gid.raw[10],
         gid.raw[11], gid.raw[12], gid.raw[13], gid.raw[14], gid.raw[15]);

  /* ───────────────────────────────────────────────────────────────────────
     STEP 5: Allocate Protection Domain
     What: ibv_alloc_pd(ctx) → allocates PD in kernel, returns userspace handle
     PD groups MRs and QPs; cross-PD access is forbidden
     ─────────────────────────────────────────────────────────────────────── */
  printf("[05] Allocating PD...\n");
  pd = ibv_alloc_pd(ctx);
  if (!pd) {
    fprintf(stderr, "Failed to alloc PD\n");
    goto cleanup_ctx;
  }

  /* ───────────────────────────────────────────────────────────────────────
     STEP 6: Create Completion Queue
     What: ibv_create_cq(ctx, cqe, context, channel, comp_vector)
     cqe = 16 → max 16 completions before overflow
     channel = NULL, comp_vector = 0 → no event channel, polling only
     ─────────────────────────────────────────────────────────────────────── */
  printf("[06] Creating CQ...\n");
  cq = ibv_create_cq(ctx, 16, NULL, NULL, 0);
  if (!cq) {
    fprintf(stderr, "Failed to create CQ\n");
    goto cleanup_pd;
  }
  printf("     CQ handle: %p\n", (void *)cq);

  /* ───────────────────────────────────────────────────────────────────────
     STEP 7: Create UD Queue Pair
     ─────────────────────────────────────────────────────────────────────── */
  printf("[07] Creating UD QP...\n");
  qp = create_ud_qp(pd, cq);
  if (!qp) {
    fprintf(stderr, "Failed to create QP\n");
    goto cleanup_cq;
  }
  printf("     QP num: 0x%x (%u)\n", qp->qp_num, qp->qp_num);

  /* ───────────────────────────────────────────────────────────────────────
     STEP 8: Transition QP: RESET → INIT → RTR → RTS
     ─────────────────────────────────────────────────────────────────────── */
  printf("[08] Transitioning QP states...\n");
  ret = modify_ud_qp_to_init(qp);
  if (ret) {
    fprintf(stderr, "INIT failed: %d\n", ret);
    goto cleanup_qp;
  }
  printf("     RESET → INIT ✓\n");

  ret = modify_ud_qp_to_rtr(qp);
  if (ret) {
    fprintf(stderr, "RTR failed: %d\n", ret);
    goto cleanup_qp;
  }
  printf("     INIT → RTR ✓\n");

  ret = modify_ud_qp_to_rts(qp);
  if (ret) {
    fprintf(stderr, "RTS failed: %d\n", ret);
    goto cleanup_qp;
  }
  printf("     RTR → RTS ✓\n");

  /* ───────────────────────────────────────────────────────────────────────
     STEP 9: Allocate and register memory
     send_buf: 64 bytes (payload only)
     recv_buf: 104 bytes (40-byte GRH + 64-byte payload)
     posix_memalign: align to 4096 (page boundary) for best DMA performance
     ─────────────────────────────────────────────────────────────────────── */
  printf("[09] Allocating memory...\n");
  if (posix_memalign(&send_buf, 4096, BUFFER_SIZE)) {
    fprintf(stderr, "posix_memalign send_buf failed\n");
    goto cleanup_qp;
  }
  size_t recv_buf_size = GRH_SIZE + BUFFER_SIZE; /* 40 + 64 = 104 */
  if (posix_memalign(&recv_buf, 4096, recv_buf_size)) {
    fprintf(stderr, "posix_memalign recv_buf failed\n");
    free(send_buf);
    goto cleanup_qp;
  }
  memset(send_buf, 0, BUFFER_SIZE);
  memset(recv_buf, 0, recv_buf_size);
  strcpy((char *)send_buf, "RDMA_UD_LOOPBACK");
  printf("     send_buf: %p, recv_buf: %p\n", send_buf, recv_buf);

  /* ───────────────────────────────────────────────────────────────────────
     STEP 10: Register Memory Regions
     ibv_reg_mr(pd, addr, length, access_flags)
     access_flags = IBV_ACCESS_LOCAL_WRITE → allow local writes to this buffer
     For recv, we need LOCAL_WRITE so driver can write incoming data
     For send, LOCAL_WRITE not strictly needed but harmless
     ─────────────────────────────────────────────────────────────────────── */
  printf("[10] Registering MRs...\n");
  mr_send = ibv_reg_mr(pd, send_buf, BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE);
  if (!mr_send) {
    fprintf(stderr, "Failed to register send MR\n");
    goto cleanup_bufs;
  }
  mr_recv = ibv_reg_mr(pd, recv_buf, recv_buf_size, IBV_ACCESS_LOCAL_WRITE);
  if (!mr_recv) {
    fprintf(stderr, "Failed to register recv MR\n");
    goto cleanup_mr_send;
  }
  printf("     MR send: lkey=0x%x, addr=%p, len=%lu\n", mr_send->lkey,
         mr_send->addr, (unsigned long)mr_send->length);
  printf("     MR recv: lkey=0x%x, addr=%p, len=%lu\n", mr_recv->lkey,
         mr_recv->addr, (unsigned long)mr_recv->length);

  /* ───────────────────────────────────────────────────────────────────────
     STEP 11: Create Address Handle for loopback
     AH specifies destination for UD send
     For loopback: destination GID = our own GID
                   destination is ourselves
     rxe0 GID index 1 = 0000:0000:0000:0000:0000:ffff:c0a8:1d9e (IPv4-mapped
     192.168.29.158)
     ─────────────────────────────────────────────────────────────────────── */
  printf("[11] Creating Address Handle (loopback)...\n");
  union ibv_gid dgid;
  if (ibv_query_gid(ctx, PORT_NUM, 1, &dgid)) {
    fprintf(stderr, "Failed to query GID index 1\n");
    goto cleanup_mr_recv;
  }
  printf("     Using GID[1]: "
         "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%"
         "02x\n",
         dgid.raw[0], dgid.raw[1], dgid.raw[2], dgid.raw[3], dgid.raw[4],
         dgid.raw[5], dgid.raw[6], dgid.raw[7], dgid.raw[8], dgid.raw[9],
         dgid.raw[10], dgid.raw[11], dgid.raw[12], dgid.raw[13], dgid.raw[14],
         dgid.raw[15]);
  struct ibv_ah_attr ah_attr;
  memset(&ah_attr, 0, sizeof(ah_attr));
  ah_attr.is_global = 1;
  ah_attr.port_num = PORT_NUM;
  ah_attr.grh.dgid = dgid;    /* destination = self (using GID index 1) */
  ah_attr.grh.sgid_index = 1; /* source GID = same index */
  ah_attr.grh.hop_limit = 64;
  ah_attr.grh.traffic_class = 0;
  ah = ibv_create_ah(pd, &ah_attr);
  if (!ah) {
    fprintf(stderr, "Failed to create AH, errno=%d\n", errno);
    goto cleanup_mr_recv;
  }
  printf("     AH handle: %p\n", (void *)ah);

  /* ───────────────────────────────────────────────────────────────────────
     STEP 12: Post receive (BEFORE send!)
     ─────────────────────────────────────────────────────────────────────── */
  printf("[12] Posting receive...\n");
  ret = post_ud_receive(qp, mr_recv, recv_buf, recv_buf_size);
  if (ret) {
    fprintf(stderr, "post_ud_receive failed: %d\n", ret);
    goto cleanup_ah;
  }

  /* ───────────────────────────────────────────────────────────────────────
     STEP 13: Post send
     remote_qpn = our own qp->qp_num (loopback)
     ─────────────────────────────────────────────────────────────────────── */
  printf("[13] Posting send...\n");
  printf("     Sending: \"%s\"\n", (char *)send_buf);
  ret = post_ud_send(qp, mr_send, send_buf, BUFFER_SIZE, ah, qp->qp_num);
  if (ret) {
    fprintf(stderr, "post_ud_send failed: %d\n", ret);
    goto cleanup_ah;
  }

  /* ───────────────────────────────────────────────────────────────────────
     STEP 14: Poll send completion
     ─────────────────────────────────────────────────────────────────────── */
  printf("[14] Polling send completion...\n");
  ret = poll_cq(cq);
  if (ret) {
    fprintf(stderr, "Send completion failed\n");
    goto cleanup_ah;
  }
  printf("     Send completed ✓\n");

  /* ───────────────────────────────────────────────────────────────────────
     STEP 15: Poll recv completion
     ─────────────────────────────────────────────────────────────────────── */
  printf("[15] Polling recv completion...\n");
  ret = poll_cq(cq);
  if (ret) {
    fprintf(stderr, "Recv completion failed\n");
    goto cleanup_ah;
  }
  printf("     Recv completed ✓\n");

  /* ───────────────────────────────────────────────────────────────────────
     STEP 16: Verify data
     recv_buf layout: [0..39] = GRH (40 bytes), [40..103] = payload (64 bytes)
     payload starts at recv_buf + GRH_SIZE = recv_buf + 40
     ─────────────────────────────────────────────────────────────────────── */
  printf("\n=== VERIFICATION ===\n");
  char *received_payload = (char *)recv_buf + GRH_SIZE;
  printf("     Sent:     \"%s\"\n", (char *)send_buf);
  printf("     Received: \"%s\"\n", received_payload);
  if (strcmp((char *)send_buf, received_payload) == 0) {
    printf("     ✓ SUCCESS: Data matches!\n");
  } else {
    printf("     ✗ FAILURE: Data mismatch\n");
  }

  /* ───────────────────────────────────────────────────────────────────────
     STEP 17: Cleanup (reverse order of allocation)
     ─────────────────────────────────────────────────────────────────────── */
  printf("\n[17] Cleanup...\n");
cleanup_ah:
  ibv_destroy_ah(ah);
cleanup_mr_recv:
  ibv_dereg_mr(mr_recv);
cleanup_mr_send:
  ibv_dereg_mr(mr_send);
cleanup_bufs:
  free(recv_buf);
  free(send_buf);
cleanup_qp:
  ibv_destroy_qp(qp);
cleanup_cq:
  ibv_destroy_cq(cq);
cleanup_pd:
  ibv_dealloc_pd(pd);
cleanup_ctx:
  ibv_close_device(ctx);

  printf("Done.\n");
  return 0;
}
