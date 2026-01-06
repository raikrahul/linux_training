/*
 * receiver.c — UDP receiver for loopback copy trace
 *
 * Binds to 127.0.0.1:9999, waits for packet, prints addresses for proof
 *
 * Build: gcc -o receiver receiver.c
 * Run: ./receiver (start BEFORE sender)
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * Get physical address from /proc/self/pagemap
 * Returns 0 on failure, physical page frame number on success
 */
uint64_t get_physical_address(void *vaddr) {
  int fd = open("/proc/self/pagemap", O_RDONLY);
  if (fd < 0) {
    perror("open pagemap");
    return 0;
  }

  uint64_t vpage = (uint64_t)vaddr / 4096;
  uint64_t offset = vpage * 8;

  if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
    perror("lseek pagemap");
    close(fd);
    return 0;
  }

  uint64_t entry;
  if (read(fd, &entry, 8) != 8) {
    perror("read pagemap");
    close(fd);
    return 0;
  }
  close(fd);

  /* Check if page is present (bit 63) */
  if (!(entry & (1ULL << 63))) {
    printf("    Page not present in RAM\n");
    return 0;
  }

  /* PFN is bits 0-54 */
  uint64_t pfn = entry & ((1ULL << 55) - 1);
  uint64_t page_offset = (uint64_t)vaddr & 0xFFF;
  uint64_t pa = (pfn << 12) | page_offset;

  return pa;
}

int main(void) {
  int fd;
  struct sockaddr_in addr, sender_addr;
  socklen_t sender_len = sizeof(sender_addr);
  char recv_buf[64];
  ssize_t n;
  struct timeval tv;

  printf(
      "════════════════════════════════════════════════════════════════════\n");
  printf("RECEIVER PROOF — ADDRESSES FOR COPY #4 TRACING\n");
  printf(
      "════════════════════════════════════════════════════════════════════\n");
  printf("PID: %d\n", getpid());

  /*
   * STEP 1: Create UDP socket
   */
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    perror("socket");
    return 1;
  }
  printf("\n[STEP 1] Socket created: fd = %d\n", fd);

  /*
   * STEP 2: Bind to 127.0.0.1:9999
   */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(9999);
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(fd);
    return 1;
  }
  printf("[STEP 2] Bound to 127.0.0.1:9999\n");

  /*
   * STEP 3: Set 90 second timeout
   */
  tv.tv_sec = 90;
  tv.tv_usec = 0;
  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    perror("setsockopt SO_RCVTIMEO");
  }
  printf("[STEP 3] Timeout set to 90 seconds\n");

  /*
   * STEP 4: Show receive buffer address BEFORE recv()
   */
  memset(recv_buf, 0, sizeof(recv_buf));

  printf("\n═══════════════════════════════════════════════════════════════════"
         "═\n");
  printf("RECEIVER BUFFER (BEFORE recv):\n");
  printf(
      "════════════════════════════════════════════════════════════════════\n");
  printf("    recv_buf VA:     %p\n", (void *)recv_buf);
  printf("    recv_buf size:   %zu bytes\n", sizeof(recv_buf));

  /* Touch the page to ensure it's mapped */
  recv_buf[0] = 'X';
  recv_buf[0] = '\0';

  uint64_t pa_before = get_physical_address(recv_buf);
  if (pa_before) {
    printf("    recv_buf PA:     0x%lx\n", pa_before);
  }
  printf("    Content:         (empty, waiting for data)\n");

  printf("\n═══════════════════════════════════════════════════════════════════"
         "═\n");
  printf("WAITING FOR PACKET... (run ./sender in another terminal)\n");
  printf(
      "════════════════════════════════════════════════════════════════════\n");

  /*
   * STEP 5: Receive data — COPY #4 happens here!
   */
  n = recvfrom(fd, recv_buf, sizeof(recv_buf) - 1, 0,
               (struct sockaddr *)&sender_addr, &sender_len);

  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      printf("ERROR: Timeout after 90 seconds, no packet received\n");
    } else {
      perror("recvfrom");
    }
    close(fd);
    return 1;
  }

  recv_buf[n] = '\0';

  printf("\n═══════════════════════════════════════════════════════════════════"
         "═\n");
  printf("PACKET RECEIVED — COPY #4 COMPLETE!\n");
  printf(
      "════════════════════════════════════════════════════════════════════\n");
  printf("    Bytes received:  %zd\n", n);
  printf("    From:            %s:%d\n", inet_ntoa(sender_addr.sin_addr),
         ntohs(sender_addr.sin_port));

  printf("\n═══════════════════════════════════════════════════════════════════"
         "═\n");
  printf("RECEIVER BUFFER (AFTER recv) — PROOF OF COPY #4:\n");
  printf(
      "════════════════════════════════════════════════════════════════════\n");
  printf("    recv_buf VA:     %p\n", (void *)recv_buf);

  uint64_t pa_after = get_physical_address(recv_buf);
  if (pa_after) {
    printf("    recv_buf PA:     0x%lx\n", pa_after);
  }

  printf("    Content:         \"%s\"\n", recv_buf);
  printf("    Hex dump:        ");
  for (int i = 0; i < n && i < 16; i++) {
    printf("%02x ", (unsigned char)recv_buf[i]);
  }
  printf("\n");

  printf("\n═══════════════════════════════════════════════════════════════════"
         "═\n");
  printf("COPY #4 PROOF:\n");
  printf(
      "════════════════════════════════════════════════════════════════════\n");
  printf("    SOURCE: Kernel skb->data (see dmesg for address)\n");
  printf("    DEST:   User VA %p → PA 0x%lx\n", (void *)recv_buf, pa_after);
  printf("    DATA:   \"%s\" (%zd bytes)\n", recv_buf, n);
  printf(
      "════════════════════════════════════════════════════════════════════\n");

  close(fd);
  return 0;
}
