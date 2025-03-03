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
- [ ] Create wally-xv6.dts
- [ ] Configure memory section
- [ ] Add UART configuration
- [ ] Add PLIC configuration
- [ ] Add SPI/SD card interface
- [ ] Compile and test device tree

## Phase 2: Storage Implementation

### 2.1 SD Card Image Creation
- [ ] Create partition scheme
  - Partition 1: Device tree (1MB)
  - Partition 2: OpenSBI (1MB)
  - Partition 3: xv6 kernel (8MB)
  - Partition 4: xv6 filesystem (remaining space)
- [ ] Modify flash-sd.sh script
  - Add xv6-specific partitioning
  - Update partition sizes
  - Add xv6 filesystem creation
  - Add verification steps
- [ ] Create test image validation tools
- [ ] Document SD card creation process

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

## Detailed Task Breakdown: SD Card Implementation

### SD Card Image Creation Tasks

1. Partition Layout Implementation
   - [ ] Calculate exact partition sizes
   - [ ] Define partition alignment requirements
   - [ ] Create GPT partition table
   - [ ] Add partition type GUIDs
   - [ ] Configure partition names

2. flash-sd.sh Script Modifications
   - [ ] Add xv6 partition scheme
   ```bash
   # Partition sizes
   PART1_SIZE=1M  # Device tree
   PART2_SIZE=1M  # OpenSBI
   PART3_SIZE=8M  # xv6 kernel
   PART4_SIZE=    # Remaining space for filesystem
   ```
   - [ ] Update sgdisk commands
   - [ ] Add filesystem creation
   - [ ] Add verification steps

3. Filesystem Creation
   - [ ] Create mkfs tool for xv6
   - [ ] Define filesystem structure
   - [ ] Add initial files and directories
   - [ ] Configure root directory

4. Image Validation
   - [ ] Create checksum verification
   - [ ] Add partition table validation
   - [ ] Test filesystem integrity
   - [ ] Verify boot sequence

5. Documentation
   - [ ] Document partition layout
   - [ ] Create setup instructions
   - [ ] Add troubleshooting guide
   - [ ] Include verification steps

### SD Card Driver Tasks

1. SPI Interface
   - [ ] Port basic SPI functions
   ```c
   // Core functions to implement
   void spi_init(void);
   uint8_t spi_transfer(uint8_t data);
   void spi_select(void);
   void spi_deselect(void);
   ```
   - [ ] Add clock configuration
   - [ ] Implement chip select handling
   - [ ] Add error detection

2. SD Card Protocol
   - [ ] Implement initialization sequence
   ```c
   // Required commands
   #define CMD0    0x40  // GO_IDLE_STATE
   #define CMD1    0x41  // SEND_OP_COND
   #define CMD16   0x50  // SET_BLOCKLEN
   #define CMD17   0x51  // READ_SINGLE_BLOCK
   #define CMD24   0x58  // WRITE_BLOCK
   ```
   - [ ] Add block read operations
   - [ ] Add block write operations
   - [ ] Implement CRC checking

3. Buffer Cache Interface
   - [ ] Define buffer structure
   - [ ] Implement buffer allocation
   - [ ] Add buffer cache
   - [ ] Implement synchronization

4. Testing Framework
   - [ ] Create unit tests
   - [ ] Add stress testing
   - [ ] Implement error injection
   - [ ] Add performance measurements

Would you like me to break down any of these tasks further or focus on implementing a specific component first?
