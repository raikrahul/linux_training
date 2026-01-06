# Execution Proof & Device Analysis

## 1. What Device Was Used?
The program output explicitly stated:
```text
Using device: rxe0
```
- **Device Name**: `rxe0`
- **Driver**: Soft-RoCE (RXE) - Software RDMA over Ethernet
- **Hardware Backing**: `wlp3s0` (Wireless LAN / WiFi)
- **Proof Source**: 
  - `cat /sys/class/infiniband/rxe0/parent` -> `wlp3s0`
  - command `ls -l /sys/class/infiniband` -> points to `virtual/infiniband/rxe0`

## 2. Status of Nvidia GPU
- **Model**: NVIDIA GeForce GTX 1650
- **Bus ID**: `00000000:01:00.0`
- **RDMA Capability**: NONE (This is a Consumer Graphics Card, not a Mellanox/Nvidia ConnectX HCA).
- **Involvement**: 0%. The GPU was strictly idle.

## 3. The Paradox
You asked for proof that the GPU is working.
**The Proof is Negative**: The program worked *despite* the GPU doing nothing.
If the GPU *was* working for RDMA, `ibv_get_device_list` would have returned a device named `mlx5_0` or similar, backed by the PCI Bus ID of the GPU/NIC.
Instead, it returned `rxe0` (Virtual Device).

## 4. Why did it work?
We "Tricked" the library.
1. `libibverbs` asked for an RDMA device.
2. Standard Hardware said "None available".
3. We loaded `rdma_rxe` module.
4. We created a **Virtual RDMA Cable** out of the WiFi card.
5. The library accepted this Virtual Cable as real hardware.
6. Memory was registered (pinned) in Main RAM (CPU RAM), not GPU VRAM.

## 5. Summary
- **Program Status**: SUCCESS (End-to-End)
- **Active Device**: WiFi Card (`wlp3s0`) pretending to be Infiniband.
- **Nvidia GPU**: Idle / Unrelated.
