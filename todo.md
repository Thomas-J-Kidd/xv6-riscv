# xv6 on Custom RISC-V Implementation Todo List

## Phase 1: Initial Setup and Configuration

### 1.1 Repository Setup
- [ ] Clone xv6-riscv repository
- [ ] Create new branch for custom implementation
- [ ] Set up build environment
- [ ] Update Makefile for RV64GC target

### 1.2 Memory Layout Configuration
- [ ] Define physical memory map
- [ ] Update kernel/memlayout.h
- [ ] Modify linker script
- [ ] Configure kernel base address

### 1.3 Device Tree Implementation
- [x] Create wally-xv6.dts
- [x] Configure memory section
- [x] Add UART configuration
- [x] Add PLIC configuration
- [x] Add SPI/SD card interface
- [x] Compile and test device tree

## Phase 2: Storage Implementation

### 2.1 SD Card Image Creation
- [x] Create partition scheme
  - Partition 1: Device tree (1MB)
  - Partition 2: OpenSBI (1MB)
  - Partition 3: xv6 kernel (8MB)
  - Partition 4: xv6 filesystem (remaining space)
- [x] Create flash-sd-xv6.sh script
  - Added xv6-specific partitioning
  - Set partition sizes
  - Added filesystem creation
  - Added verification steps
- [x] Create device tree (wally-xv6.dts)
- [x] Document SD card creation process in README.md

### 2.2 SD Card Driver Implementation
- [ ] Port SPI driver from ZSBL
  - Review current spi.c/spi.h
  - Identify reusable components
  - Create xv6-compatible interface
- [ ] Implement SD card protocol
  - Basic initialization
  - Block read operations
  - Block write operations
  - Error handling
- [ ] Create buffer cache interface
- [ ] Add testing framework

## Phase 3: Boot Process Implementation

### 3.1 ZSBL Modifications
- [ ] Update ZSBL for xv6 loading
- [ ] Implement GPT parsing
- [ ] Add device tree loading
- [ ] Configure OpenSBI loading
- [ ] Add xv6 kernel loading
- [ ] Implement boot handoff

### 3.2 OpenSBI Configuration
- [ ] Review current OpenSBI setup
- [ ] Update for xv6 requirements
- [ ] Configure supervisor mode handoff
- [ ] Test SBI calls

### 3.3 Kernel Boot Process
- [ ] Implement start.c
- [ ] Configure trap handling
- [ ] Set up memory protection
- [ ] Initialize devices
- [ ] Configure console

## Phase 4: Device Support

### 4.1 UART Implementation
- [ ] Port UART driver
- [ ] Configure console I/O
- [ ] Add printf support
- [ ] Test basic I/O

### 4.2 PLIC Configuration
- [ ] Configure interrupt controller
- [ ] Set up interrupt handling
- [ ] Test device interrupts

## Phase 5: Testing and Validation

### 5.1 Unit Tests
- [ ] Create SD card tests
- [ ] Add boot sequence tests
- [ ] Test device drivers
- [ ] Validate memory operations

### 5.2 Integration Tests
- [ ] Test full boot sequence
- [ ] Validate filesystem operations
- [ ] Test user processes
- [ ] Verify device interactions

### 5.3 Performance Testing
- [ ] Measure boot time
- [ ] Test filesystem performance
- [ ] Analyze memory usage
- [ ] Profile interrupt handling

## Next Steps

1. Clone and configure xv6-riscv repository
2. Port SPI driver from ZSBL to xv6
3. Update ZSBL for xv6 boot process
4. Configure OpenSBI for xv6

## Completed Tasks

1. Created SD card tools:
   - flash-sd-xv6.sh script
   - Device tree (wally-xv6.dts)
   - Documentation (README.md)
   - Makefile for building tools

2. Configured device tree with:
   - Memory layout (128MB at 0x80000000)
   - UART at 0x10000000
   - PLIC at 0x0C000000
   - CLINT at 0x02000000
   - SPI/SD card interface at 0x13000
