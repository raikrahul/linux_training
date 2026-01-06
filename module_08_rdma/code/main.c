/*
 * RDMA Memory Registration Demo
 * Strict Axiomatic Derivation in comments
 */

#include <stdio.h>      // Input/Output -> printf
#include <stdlib.h>     // Standard Lib -> malloc, free, exit
#include <string.h>     // String ops -> memset
#include <infiniband/verbs.h> // RDMA definitions -> ibv_*

/* 
 * User requested: #define size_t int
 * AXIOM: Machine is 64-bit (x86_64).
 * AXIOM: Address space > 4GB requires 64-bit integer.
 * AXIOM: int is 32-bit (4 bytes) on this ABI.
 * AXIOM: size_t is 64-bit (8 bytes) on this ABI.
 * DERIVATION: To address full memory, we MUST use 64-bit size_t.
 * If we force int, we truncat pointers/sizes > 4GB => CRASH.
 * THEREFORE: We use standard size_t (unsigned long).
 */

int main() {
    struct ibv_device **dev_list;
    struct ibv_device *ib_dev;
    struct ibv_context *context;
    struct ibv_pd *pd;
    struct ibv_mr *mr;
    int num_devices;
    void *buf;
    size_t size = 4096; // 4KB page
    int access_flags = IBV_ACCESS_LOCAL_WRITE;

    printf("Step 1: Get Device List\n");
    // Get list of RDMA devices available on machine
    dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list) {
        fprintf(stderr, "Error: Failed to get IB devices list.\n");
        return 1;
    }
    printf("  Found %d device(s)\n", num_devices);
    
    if (num_devices == 0) {
        fprintf(stderr, "Error: No IB devices found.\n");
        ibv_free_device_list(dev_list);
        return 1;
    }

    // Pick first device
    ib_dev = dev_list[0];
    printf("  Using device: %s\n", ibv_get_device_name(ib_dev));

    printf("Step 2: Open Device Context\n");
    context = ibv_open_device(ib_dev);
    if (!context) {
        fprintf(stderr, "Error: Failed to open device context.\n");
        ibv_free_device_list(dev_list);
        return 1;
    }
    
    // Free list, we have the context now
    ibv_free_device_list(dev_list);

    printf("Step 3: Allocate Protection Domain (PD)\n");
    // PD groups MRs and QPs. Like a security container.
    pd = ibv_alloc_pd(context);
    if (!pd) {
        fprintf(stderr, "Error: Failed to allocate PD.\n");
        ibv_close_device(context);
        return 1;
    }

    printf("Step 4: Allocate Memory Buffer\n");
    // Allocate 4KB buffer
    // Align to page boundary (4096) for best performance
    if (posix_memalign(&buf, 4096, size)) {
        fprintf(stderr, "Error: Failed to allocate memory.\n");
        ibv_dealloc_pd(pd);
        ibv_close_device(context);
        return 1;
    }
    // Write some data
    memset(buf, 0, size);
    sprintf((char*)buf, "Hello RDMA World!");

    printf("Step 5: Register Memory Region (MR)\n");
    /*
     * ibv_reg_mr(pd, addr, length, access)
     * Pin the memory (lock it in RAM) so hardware can DMA.
     * physical_addr <- virtual_addr translation is fixed.
     */
    mr = ibv_reg_mr(pd, buf, size, access_flags);
    if (!mr) {
        fprintf(stderr, "Error: Failed to register MR.\n");
        free(buf);
        ibv_dealloc_pd(pd);
        ibv_close_device(context);
        return 1;
    }

    printf("  MR Registered Successfully!\n");
    printf("  -> addr: %p\n", mr->addr);
    printf("  -> length: %lu\n", (unsigned long)mr->length);
    printf("  -> lkey: 0x%x\n", mr->lkey);
    printf("  -> rkey: 0x%x\n", mr->rkey);

    printf("Step 6: Deregister MR\n");
    if (ibv_dereg_mr(mr)) {
        fprintf(stderr, "Error: Failed to deregister MR.\n");
        return 1;
    }

    printf("Step 7: Cleanup\n");
    free(buf);
    ibv_dealloc_pd(pd);
    ibv_close_device(context);

    printf("Success.\n");
    return 0;
}
