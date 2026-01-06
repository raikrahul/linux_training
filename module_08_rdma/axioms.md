[00] Machine has CPU and RAM.
[01] RAM is array of bytes. Index is "Address".
[02] CPU uses 64-bit registers. Max Address 2^64 - 1.
[03] C type `int` is 4 bytes (32-bit). Max Value 2^31-1 (signed).
[04] C type `size_t` is 8 bytes (64-bit). Max Value 2^64-1 (unsigned).
[05] User requested `#define size_t int`.
[06] RAM size can exceed 4GB (2^32 bytes).
[07] If `size_t` is `int`, we cannot represent length > 4GB.
[08] `libibverbs` is compiled expecting 8-byte `length` argument.
[09] Passing 4-byte `int` (User `size_t`) to 8-byte slot reads garbage/stack corruption.
[10] ∴ We MUST use standard `size_t` (8 bytes) for "End-to-End" success.

[11] `ibv_get_device_list` probes `/sys/class/infiniband`.
[12] Initially `ibv_get_device_list` returned 0.
[13] `rdma_rxe` module emulates RDMA over Ethernet (UDP).
[14] `rdma link add` binds `rxe0` to `wlp3s0`.
[15] Now `/sys/class/infiniband` contains `rxe0`.
[16] `ibv_get_device_list` returns 1 (`rxe0`).

[17] `posix_memalign` gives Virtual Address (VA) → 0x56936ab9a000.
[18] `ibv_reg_mr` calls Kernel (`uverbs`).
[19] Kernel locks RAM pages (Physical Address).
[20] Device (`rxe0`) assigns handle `lkey` (0x292).
[21] ∴ VA 0x56936ab9a000 + Length 4096 is now accessible via LKey 0x292.
