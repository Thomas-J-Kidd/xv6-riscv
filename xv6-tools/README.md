# xv6 Tools for Wally RISC-V

This directory contains tools for building and flashing xv6 to the Wally RISC-V implementation on the ArtyA7 FPGA.

## Components

- `flash-sd-xv6.sh`: Script to create bootable SD card for xv6
- `wally-xv6.dts`: Device tree source for Wally hardware configuration
- `wally-xv6.dtb`: Compiled device tree blob
- `Makefile`: For building device tree and other tools

## SD Card Layout

The script creates a GPT-partitioned SD card with:

1. Device Tree (1MB)
2. OpenSBI Firmware (1MB)
3. xv6 Kernel (8MB)
4. xv6 Filesystem (remaining space)

## Usage

1. Build xv6:
   ```bash
   cd path/to/xv6-riscv
   make
   ```

2. Compile device tree:
   ```bash
   cd xv6-tools
   make
   ```

3. Flash SD card:
   ```bash
   # Replace /dev/sdX with your SD card device
   ./flash-sd-xv6.sh -x path/to/xv6-riscv -d wally-xv6.dtb /dev/sdX
   ```

   Options:
   - `-x`: Path to xv6 directory
   - `-d`: Path to device tree file
   - `-z`: Wipe card before partitioning (optional)

4. Insert SD card into ArtyA7's PMOD SD card slot and boot

## Hardware Requirements

- ArtyA7 FPGA board
- Micro SD card (at least 32MB)
- PMOD SD card adapter
- USB cable for UART connection

## Directory Structure Expected by flash-sd-xv6.sh

```
xv6-riscv/
├── kernel/
│   └── kernel     # xv6 kernel binary
└── firmware/
    └── fw_jump.bin # OpenSBI firmware
```

## Notes

- The script requires root privileges to write to the SD card
- Make sure to specify the correct device path to avoid data loss
- The script will prompt for confirmation before writing to the device
