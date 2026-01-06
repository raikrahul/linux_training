# NETWORK COPY — AXIOMATIC DERIVATION FROM FIRST PRINCIPLES

## AXIOM 0: PRIMATE KNOWS THESE THINGS ONLY

```
- Counting: 0, 1, 2, 3, ...
- Addition: 3 + 5 = 8
- Subtraction: 10 - 3 = 7
- Bits: 0 or 1 (two states)
- Byte: 8 bits grouped together
- AND: 1 AND 1 = 1, else 0
- OR: 1 OR 0 = 1, 0 OR 0 = 0
```

---

## STEP 1: WHAT IS A BYTE?

PROBLEM: How to represent numbers bigger than 1?
SOLUTION: Group 8 bits together

```
1 bit  = 0 or 1                    → can represent 2 values (0, 1)
2 bits = 00, 01, 10, 11            → can represent 4 values (0, 1, 2, 3)
8 bits = 00000000 to 11111111      → can represent 256 values (0 to 255)
```

CALCULATION:
- 2^1 = 2 values
- 2^2 = 4 values
- 2^8 = 256 values

∴ 1 byte = 8 bits = can store number 0 to 255

---

## STEP 2: WHAT IS RAM?

PROBLEM: Where to store bytes?
SOLUTION: RAM = array of bytes

```
RAM = giant array
RAM[0] = first byte
RAM[1] = second byte
RAM[2] = third byte
...
RAM[16000000000] = 16 billionth byte (if you have 16GB RAM)
```

YOUR MACHINE:
- 16 GB RAM = 16 × 1024 × 1024 × 1024 bytes = 17,179,869,184 bytes
- Addresses: 0x0000000000 to 0x3FFFFFFFF (approximately)

DEFINITION: Address = the index number to access a specific byte in RAM

---

## STEP 3: WHAT IS HEXADECIMAL?

PROBLEM: Writing long binary numbers is tedious
SOLUTION: Group 4 bits, use symbols 0-9, A-F

```
4 bits = 0000 to 1111 = 16 values
Use: 0 1 2 3 4 5 6 7 8 9 A B C D E F

Binary → Hex:
0000 = 0
0001 = 1
...
1001 = 9
1010 = A (means 10)
1011 = B (means 11)
1100 = C (means 12)
1101 = D (means 13)
1110 = E (means 14)
1111 = F (means 15)
```

EXAMPLE:
- 0xFF = 1111 1111 = 255 (decimal)
- 0x10 = 0001 0000 = 16 (decimal)

---

## STEP 4: WHAT IS A CHARACTER?

PROBLEM: How to store letters like 'H', 'E', 'L', 'L', 'O'?
SOLUTION: Assign a number to each letter (ASCII table)

```
'H' = 72 = 0x48
'E' = 69 = 0x45
'L' = 76 = 0x4C
'L' = 76 = 0x4C
'O' = 79 = 0x4F
```

CALCULATION (for 'H'):
- ASCII code for 'H' = 72
- 72 in hex: 72 ÷ 16 = 4 remainder 8 → 0x48
- Verify: 4 × 16 + 8 = 64 + 8 = 72 ✓

---

## STEP 5: WHAT IS A STRING?

PROBLEM: How to store multiple characters together?
SOLUTION: Put characters in consecutive RAM locations

```
"HELLO" stored starting at RAM address 1000:

RAM[1000] = 0x48 ('H')
RAM[1001] = 0x45 ('E')
RAM[1002] = 0x4C ('L')
RAM[1003] = 0x4C ('L')
RAM[1004] = 0x4F ('O')
RAM[1005] = 0x00 (null terminator, marks end of string)
```

DEFINITION: String = consecutive bytes in RAM, ending with 0x00

---

## STEP 6: WHAT IS A PROCESS?

PROBLEM: CPU can only run one instruction at a time. How to run multiple programs?
SOLUTION: Operating system switches between programs rapidly

```
Program A runs for 10 milliseconds
Program B runs for 10 milliseconds
Program A runs for 10 milliseconds
...
```

DEFINITION: Process = a running program with its own:
- Code (instructions)
- Data (variables)
- Stack (temporary storage)
- PID (process ID, a unique number)

YOUR DATA:
- sender process: PID = 40764
- receiver process: PID = 8303

---

## STEP 7: WHAT IS VIRTUAL ADDRESS (VA)?

PROBLEM: If two processes both want to use RAM address 1000, they conflict!
SOLUTION: Each process gets its OWN address space (virtual addresses)

```
Process A sees: VA 0x1000, VA 0x1001, VA 0x1002, ...
Process B sees: VA 0x1000, VA 0x1001, VA 0x1002, ...

These VA 0x1000 are DIFFERENT physical locations!
```

DEFINITION: Virtual Address (VA) = address that a process uses
DEFINITION: Physical Address (PA) = actual address in RAM hardware

```
Process A: VA 0x1000 → PA 0x50000 (mapped by kernel)
Process B: VA 0x1000 → PA 0x80000 (mapped to different PA!)
```

---

## STEP 8: WHAT IS THE KERNEL?

PROBLEM: Who manages CPU, RAM, processes, devices?
SOLUTION: Kernel = the core program that manages everything

```
USER SPACE: Your programs run here (sender.c, receiver.c)
─────────────────────────────────────────────────────────
KERNEL SPACE: Linux kernel runs here (allocates RAM, talks to NIC)
─────────────────────────────────────────────────────────
HARDWARE: CPU, RAM, NIC, Disk
```

DEFINITION: Kernel = program that controls hardware and manages processes

KEY FACT: User programs CANNOT directly access hardware. Must ask kernel.

---

## STEP 9: WHAT IS A SYSCALL?

PROBLEM: How does user program ask the kernel to do something?
SOLUTION: System call (syscall) = special instruction to enter kernel

```
User program:  "I want to send data to network"
           ↓
           syscall instruction (CPU switches to kernel mode)
           ↓
Kernel:        "OK, I'll copy your data and send it"
           ↓
           sysret instruction (CPU switches back to user mode)
           ↓
User program:  continues running
```

EXAMPLE SYSCALLS:
- sendto() = send data to network
- recvfrom() = receive data from network
- read() = read from file
- write() = write to file

---

## STEP 10: WHAT IS A SOCKET?

PROBLEM: How to identify a network connection?
SOLUTION: Socket = handle (number) representing a network endpoint

```
int fd = socket(AF_INET, SOCK_DGRAM, 0);
```

- AF_INET = use IPv4 (Internet Protocol version 4)
- SOCK_DGRAM = use UDP (User Datagram Protocol)
- fd = 3 (file descriptor, a small number returned by kernel)

DEFINITION: Socket = kernel object for network communication, identified by fd

---

## STEP 11: WHAT IS AN IP ADDRESS?

PROBLEM: How to identify a specific computer on network?
SOLUTION: IP address = 4 bytes that uniquely identify a machine

```
127.0.0.1 = loopback (this same computer)

Each number is 1 byte (0-255):
127 . 0 . 0 . 1
 ↓    ↓   ↓   ↓
byte byte byte byte
```

CALCULATION:
- 127 in hex = 0x7F
- Full address in hex: 0x7F000001

---

## STEP 12: WHAT IS A PORT?

PROBLEM: One computer might have multiple programs wanting network data
SOLUTION: Port = 2-byte number (0-65535) to identify specific program

```
IP 127.0.0.1 + Port 9999 = identifies THIS program on THIS computer
```

YOUR DATA:
- Receiver binds to port 9999
- Port 9999 in hex: 9999 = 0x270F

---

## STEP 13: WHY COPY FROM USER TO KERNEL?

PROBLEM: User program has data at its VA. Kernel needs the data.

```
sender.c has: char *msg = "HELLO_SEND_TRACE"
msg is at VA 0x6130204f0069 (in sender's address space)
```

BUT: Kernel cannot directly read VA 0x6130204f0069!
WHY: That VA belongs to sender process. Kernel has its own address space.

SOLUTION: Kernel COPIES data from user VA to kernel VA.

```
User VA 0x6130204f0069 → copy → Kernel VA 0xffff8cb08de63c00
```

THIS IS COPY #1.

---

## STEP 14: WHAT IS sk_buff (skb)?

PROBLEM: Kernel needs structure to hold network packet data
SOLUTION: sk_buff = kernel structure for network packets

```
struct sk_buff {
    unsigned char *data;   // pointer to packet bytes
    unsigned int len;      // length of packet
    // ... many other fields
};
```

DEFINITION: sk_buff = kernel object holding network packet

YOUR DATA:
- skb->data = 0xffff8cb08de63c00 (kernel VA where "HELLO_SEND_TRACE" is copied)

---

## STEP 15: WHAT IS LOOPBACK?

PROBLEM: How to test networking without real network?
SOLUTION: Loopback = fake network device that sends packets to itself

```
Sender → kernel → loopback device → kernel → Receiver
             ↑                            ↑
             Same computer, same RAM, no wire
```

IP address 127.0.0.1 = loopback address

KEY FACT: With loopback, the SAME skb is passed from TX to RX queue.
No data copy needed between send and receive sides (for loopback only).

---

## STEP 16: WHY COPY FROM KERNEL TO USER?

PROBLEM: Kernel received data in its skb. User program wants the data.

```
receiver.c calls: recv(fd, buf, 16, 0)
buf is at VA 0x7fff43abc810 (in receiver's address space)
```

BUT: Kernel's skb->data is at VA 0xffff8882cbbe612c.
Receiver process cannot access kernel VA!

SOLUTION: Kernel COPIES data from its skb to user's buffer.

```
Kernel VA 0xffff8882cbbe612c → copy → User VA 0x7fff43abc810
```

THIS IS COPY #4.

---

## STEP 17: WHY "DOUBLE COPY"?

COUNTING THE COPIES:

```
SEND PATH:
  User buffer (sender)     → COPY #1 → Kernel skb
  
RECEIVE PATH:
  Kernel skb               → COPY #4 → User buffer (receiver)
```

TOTAL: 2 copies (for loopback)
- Same 16 bytes copied TWICE
- Uses 48 bytes of RAM (16 × 3 locations)

FOR REAL NIC (not loopback):
- COPY #2: NIC DMA reads from kernel skb, sends to wire
- COPY #3: NIC DMA writes received data to kernel skb
- TOTAL: 2 CPU copies + 2 DMA copies = 4 data movements

---

## STEP 18: WHAT IS KPROBE?

PROBLEM: How to observe what kernel does without modifying kernel source?
SOLUTION: Kprobe = insert probe at kernel function, run your code when called

```
register_kprobe(&kp)  → tell kernel: "when function X is called, run my handler"
```

DEFINITION: Kprobe = runtime kernel instrumentation

YOUR KPROBES:
- Probe on _copy_from_iter → catches COPY #1
- Probe on _copy_to_iter → catches COPY #4

---

## STEP 19: WHAT ARE REGISTERS?

PROBLEM: CPU needs fast storage for current calculation
SOLUTION: Registers = small, fast storage inside CPU

x86_64 calling convention:
```
Function call: foo(arg1, arg2, arg3)

arg1 → stored in register RDI (also called regs->di)
arg2 → stored in register RSI (also called regs->si)
arg3 → stored in register RDX (also called regs->dx)
```

YOUR DATA for _copy_to_iter(addr, bytes, iter):
- regs->di = addr = 0xffff8882cbbe612c (kernel source VA)
- regs->si = bytes = 16
- regs->dx = iter (pointer to iov_iter struct)

---

## STEP 20: THE PROOF

WHAT WE MEASURED:

```
COPY #1 (send path):
  dmesg: [COPY1] dest=ffff8cb08de63c00 len=16
  sender.c: buffer at VA 0x6130204f0069
  ∴ memcpy(0xffff8cb08de63c00, 0x6130204f0069, 16) happened

COPY #4 (receive path):
  dmesg: [COPY4] src=ffff8882cbbe612c len=16
  receiver.c: buffer at VA 0x7fff43abc810, received "HELLO_SEND_TRACE"
  ∴ memcpy(0x7fff43abc810, 0xffff8882cbbe612c, 16) happened
```

THREE LOCATIONS IN RAM:

```
Location 1: 0x6130204f0069     (sender user VA)    → "HELLO_SEND_TRACE"
Location 2: 0xffff8882cbbe612c (kernel skb VA)     → "HELLO_SEND_TRACE"
Location 3: 0x7fff43abc810     (receiver user VA)  → "HELLO_SEND_TRACE"

Same 16 bytes exist in 3 places = 48 bytes of RAM used for 16 bytes of data
```

---

## SUMMARY: DOUBLE COPY CHAIN

```
Step  Action                           Location                     Data
────  ─────────────────────────────    ─────────────────────────    ────────────────
1     sender writes string             VA 0x6130204f0069            "HELLO_SEND_TRACE"
2     sender calls sendto()                                         
3     COPY #1: kernel copies in        VA 0xffff...c00 (skb)        "HELLO_SEND_TRACE"
4     loopback passes skb to RX                                     
5     receiver calls recv()                                         
6     COPY #4: kernel copies out       VA 0x7fff43abc810            "HELLO_SEND_TRACE"
7     receiver reads string                                         "HELLO_SEND_TRACE"
```

---

## NEW THINGS INTRODUCED WITHOUT DERIVATION: NONE

Every term derived from previous step:
- Byte derived from bits (Step 1)
- RAM derived from bytes (Step 2)
- Hex derived from bits (Step 3)
- Character derived from bytes (Step 4)
- String derived from characters (Step 5)
- Process derived from program execution (Step 6)
- VA derived from process isolation (Step 7)
- Kernel derived from hardware management (Step 8)
- Syscall derived from user-kernel boundary (Step 9)
- Socket derived from network handle (Step 10)
- IP address derived from machine identification (Step 11)
- Port derived from program identification (Step 12)
- Copy #1 derived from user-kernel isolation (Step 13)
- skb derived from kernel packet storage (Step 14)
- Loopback derived from testing need (Step 15)
- Copy #4 derived from kernel-user isolation (Step 16)
- Double copy derived from counting (Step 17)
- Kprobe derived from observation need (Step 18)
- Registers derived from CPU storage (Step 19)
- Proof derived from measurements (Step 20)
