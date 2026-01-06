/*
 * sender.c — UDP sender for network copy tracing exercise
 *
 * AXIOM CHAIN:
 * 001. socket(AF_INET, SOCK_DGRAM, 0) → creates UDP socket → returns fd (file
 * descriptor) 002. sendto(fd, buf, len, flags, dest_addr, addrlen) → sends data
 * to destination 003. Inside kernel: buf (user VA) → copied to skb->data
 * (kernel VA) → COPY #1 004. skb passed to device driver → data moved to NIC →
 * COPY #2
 *
 * YOUR TASK: Run this while kernel module traces copy_from_iter and
 * dev_queue_xmit
 *
 * COMPILE: gcc -Wall -g -o sender sender.c
 * RUN: ./sender
 *
 * Machine data:
 * - Loopback: 127.0.0.1
 * - Port: 9999
 * - Message: "HELLO_SEND_TRACE" (16 bytes)
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 9999
#define MESSAGE "HELLO_SEND_TRACE"

int main(void) {
  int fd;
  struct sockaddr_in dest;
  ssize_t sent;

  /*
   * STEP 1: Create UDP socket
   * socket(domain, type, protocol)
   * AF_INET = IPv4 (value = 2)
   * SOCK_DGRAM = UDP (value = 2)
   * protocol = 0 (default for SOCK_DGRAM = UDP = 17)
   */
  printf("[1] Creating UDP socket...\n");
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    perror("socket");
    return 1;
  }
  printf("    fd = %d\n", fd);

  /*
   * STEP 2: Setup destination address
   * sin_family = AF_INET (2)
   * sin_port = htons(9999) = 0x270F in network byte order
   * sin_addr = inet_addr("127.0.0.1") = 0x7F000001 = 2130706433
   */
  printf("[2] Setting destination: 127.0.0.1:%d\n", PORT);
  memset(&dest, 0, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_port = htons(PORT);
  dest.sin_addr.s_addr = inet_addr("127.0.0.1");

  /*
   * STEP 3: Send message
   * sendto(fd, buf, len, flags, dest_addr, addrlen)
   * buf = &MESSAGE[0] (user VA on stack or rodata)
   * len = 16 bytes
   * flags = 0
   *
   * KERNEL WILL:
   * - copy MESSAGE from user VA to kernel skb->data (COPY #1)
   * - pass skb to loopback driver (COPY #2 or just pointer move?)
   *
   * YOUR KPROBE SHOULD TRACE THIS!
   */
  printf("[3] Sending: \"%s\" (%zu bytes)\n", MESSAGE, strlen(MESSAGE));
  printf("    Buffer address (user VA): %p\n", (void *)MESSAGE);

  sent = sendto(fd, MESSAGE, strlen(MESSAGE), 0, (struct sockaddr *)&dest,
                sizeof(dest));

  if (sent < 0) {
    perror("sendto");
    close(fd);
    return 1;
  }
  printf("    Sent %zd bytes ✓\n", sent);

  /*
   * STEP 4: Cleanup
   */
  printf("[4] Closing socket...\n");
  close(fd);

  printf("\nDone. Check dmesg for kprobe output.\n");
  return 0;
}
