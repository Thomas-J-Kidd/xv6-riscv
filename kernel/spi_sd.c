// spi_sd.c
// SD Card Driver for xv6 using Wally SPI Controller

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#include <stdint.h>

// --- Forward Declarations for Static Functions ---
static void delay_ms(uint ms);
static inline uint64 get_cycles(void);
static inline int check_timeout(uint64 start_cycle, uint64 timeout_cycles);
static inline void write_spi_reg(uintptr_t addr, uint32_t value);
static inline uint32_t read_spi_reg(uintptr_t addr);
static void spi_set_clock(uint32_t clkout_hz);
static uint8_t spi_transfer(uint8_t tx);
static void spi_dummy_bytes(int n);
static void sd_select(void);
static void sd_deselect(void);
static uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc);
static int sd_cmd_r7(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t *r1_res, uint8_t resp_buf[4]);


// --- Configuration ---
#define PCLK_HZ         200000000 // Peripheral Clock Frequency (Adjust if different!)
#define SPI_HZ_INIT     400000    // SD Card Init Speed (<= 400 kHz)
#define SPI_HZ_FAST     20000000  // SD Card Fast Speed (<= 25 MHz typical, check card)
#define SPI_CS_ID       0         // Chip Select ID for SD Card
#define SPI_TIMEOUT_MS  1000      // Timeout for SPI operations in ms (approx)

// --- SPI Controller Registers (Based on Wally Docs, Base 0x10040000) ---
#define SPI_BASE        0x10040000
#define SPI_SCKDIV      (SPI_BASE + 0x00) /* Serial clock divisor */
#define SPI_SCKMODE     (SPI_BASE + 0x04) /* Serial clock mode */
#define SPI_CSID        (SPI_BASE + 0x10) /* Chip select ID */
#define SPI_CSDEF       (SPI_BASE + 0x14) /* Chip select default */
#define SPI_CSMODE      (SPI_BASE + 0x18) /* Chip select mode */
#define SPI_DELAY0      (SPI_BASE + 0x28) /* Delay control 0 */
#define SPI_DELAY1      (SPI_BASE + 0x2c) /* Delay control 1 */
#define SPI_FMT         (SPI_BASE + 0x40) /* Frame format */
#define SPI_TXDATA      (SPI_BASE + 0x48) /* Tx FIFO data */
#define SPI_RXDATA      (SPI_BASE + 0x4c) /* Rx FIFO data */
#define SPI_TXMARK      (SPI_BASE + 0x50) /* Tx FIFO watermark */
#define SPI_RXMARK      (SPI_BASE + 0x54) /* Rx FIFO watermark */
#define SPI_IE          (SPI_BASE + 0x70) /* Interrupt Enable Register */
#define SPI_IP          (SPI_BASE + 0x74) /* Interrupt Pendings Register */

// SPI_FMT bits
#define SPI_FMT_PROTO(x)  ((x) << 0)  // 0=single, 1=dual, 2=quad
#define SPI_FMT_ENDIAN(x) ((x) << 2)  // 0=big, 1=little
#define SPI_FMT_DIR(x)    ((x) << 3)  // 0=rx/tx, 1=rx only
#define SPI_FMT_LEN(x)    (((x)-1) << 16) // bits per frame (1-8)

// SPI_SCKMODE bits
#define SPI_SCKMODE_PHA(x) ((x) << 0) // Phase
#define SPI_SCKMODE_POL(x) ((x) << 1) // Polarity

// SPI_CSMODE bits
#define SPI_CSMODE_AUTO   0U
#define SPI_CSMODE_HOLD   2U
#define SPI_CSMODE_OFF    3U

// SPI_TXDATA / SPI_RXDATA flags
#define SPI_FIFO_FULL     (1U << 31)
#define SPI_FIFO_EMPTY    (1U << 31)

// --- SD Card Commands ---
#define CMD0    (0)         // GO_IDLE_STATE
#define CMD8    (8)         // SEND_IF_COND
#define CMD9    (9)         // SEND_CSD
#define CMD10   (10)        // SEND_CID
#define CMD12   (12)        // STOP_TRANSMISSION
#define CMD16   (16)        // SET_BLOCKLEN
#define CMD17   (17)        // READ_SINGLE_BLOCK
#define CMD18   (18)        // READ_MULTIPLE_BLOCK
#define CMD24   (24)        // WRITE_BLOCK
#define CMD25   (25)        // WRITE_MULTIPLE_BLOCK
#define CMD55   (55)        // APP_CMD
#define CMD58   (58)        // READ_OCR
#define ACMD41  (41)        // SD_SEND_OP_COND (ACMD)

// --- SD Card Responses/Tokens ---
#define R1_IDLE_STATE           (1 << 0)
#define R1_ERASE_RESET          (1 << 1)
#define R1_ILLEGAL_COMMAND      (1 << 2)
#define R1_COM_CRC_ERROR        (1 << 3)
#define R1_ERASE_SEQUENCE_ERROR (1 << 4)
#define R1_ADDRESS_ERROR        (1 << 5)
#define R1_PARAMETER_ERROR      (1 << 6)
// R1 bit 7 is always 0
#define START_BLOCK_TOKEN       0xFE
#define WRITE_ACCEPTED_MASK     0x1F
#define WRITE_ACCEPTED          0x05 // Data accepted
#define WRITE_CRC_ERROR         0x0B // Data rejected due to CRC
#define WRITE_ERROR             0x0D // Data rejected due to write error

// --- Helper Macros ---
// Simple delay loop (approximate, depends on CPU speed)
static void delay_ms(uint ms) {
    // Very rough calibration needed here. Assume PCLK_HZ gives ~cycles/sec
    // This is highly inaccurate but provides a basic delay mechanism.
    volatile uint64 delay_count = (uint64)ms * (PCLK_HZ / 10000); // Adjust divisor as needed
    while (delay_count-- > 0);
}

// Crude timeout mechanism based on cycles
// #define TIMEOUT_CYCLES (PCLK_HZ / 1000 * SPI_TIMEOUT_MS) // Adjust calculation based on timer frequency if available
static inline uint64 get_cycles(void) {
    uint64 ret;
    asm volatile("rdcycle %0" : "=r"(ret));
    return ret;
}

static inline int check_timeout(uint64 start_cycle, uint64 timeout_cycles) {
    // This needs a proper timer peripheral or assumes rdcycle wraps predictably
    // For simplicity, just return 0 (no timeout) - ADD PROPER TIMEOUTS IF NEEDED
    // Example: return (get_cycles() - start_cycle) > timeout_cycles;
    return 0; // WARNING: Timeout disabled
}


// --- SPI Low-Level Functions ---
static inline void write_spi_reg(uintptr_t addr, uint32_t value) {
  *(volatile uint32_t *) addr = value;
}

static inline uint32_t read_spi_reg(uintptr_t addr) {
  return *(volatile uint32_t *) addr;
}

// Set SPI clock speed
static void spi_set_clock(uint32_t clkout_hz) {
  uint32_t div = (PCLK_HZ / (2 * clkout_hz)) - 1;
  // Clamp divisor if needed (register is 12 bits wide in docs)
  if (div > 0xFFF) div = 0xFFF;
  write_spi_reg(SPI_SCKDIV, div);
  printf("spi: set clock divisor to %d for %d Hz\n", div, clkout_hz);
}

// Transfer one byte (send tx, return rx)
static uint8_t spi_transfer(uint8_t tx) {
  uint64 start = get_cycles(); // For timeout

  // Wait for TX FIFO to have space
  while (read_spi_reg(SPI_TXDATA) & SPI_FIFO_FULL) {
      if (check_timeout(start, 1000000)) { // Example timeout cycles
          panic("spi_transfer: TX timeout");
      }
  }

  // Send byte
  write_spi_reg(SPI_TXDATA, tx);

  // Wait for RX FIFO to have data
  uint32_t rx;
  while ((rx = read_spi_reg(SPI_RXDATA)) & SPI_FIFO_EMPTY) {
      if (check_timeout(start, 2000000)) { // Example timeout cycles
          panic("spi_transfer: RX timeout");
      }
  }

  // Return received byte
  return (uint8_t)rx;
}

// Send dummy bytes (typically 0xFF)
static void spi_dummy_bytes(int n) {
  for (int i = 0; i < n; i++) {
    spi_transfer(0xFF);
  }
}

// --- SD Card Protocol Functions ---

// Select card (assert CS)
static void sd_select() {
  write_spi_reg(SPI_CSMODE, SPI_CSMODE_HOLD); // Hold CS low during command/data
  write_spi_reg(SPI_CSID, SPI_CS_ID);
  spi_dummy_bytes(1); // Allow CS to settle
}

// Deselect card (deassert CS)
static void sd_deselect() {
  spi_dummy_bytes(1); // Ensure last byte clocks out
  write_spi_reg(SPI_CSMODE, SPI_CSMODE_AUTO); // Return to auto mode
  // The CS line should go high based on CSDEF when controller is idle in AUTO mode.
  // Or set CSID to an invalid ID if necessary? Let's assume AUTO works.
  spi_dummy_bytes(1); // Give card time
}

// Send SD command and get R1 response
// Returns R1 response or 0xFF on timeout/error
static uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
  uint8_t r1 = 0xFF;
  int retry = 0;

  spi_transfer(cmd | 0x40); // Command index + start bit + transmission bit
  spi_transfer((uint8_t)(arg >> 24));
  spi_transfer((uint8_t)(arg >> 16));
  spi_transfer((uint8_t)(arg >> 8));
  spi_transfer((uint8_t)arg);
  spi_transfer(crc); // CRC (often ignored except for CMD0/CMD8)

  // Wait for response (first byte is not 0xFF)
  // Max 8 dummy reads according to spec
  for (retry = 0; retry < 8; retry++) {
    r1 = spi_transfer(0xFF);
    if (r1 != 0xFF) break;
  }
  return r1;
}

// Send command and get R1 + 4 bytes response (R3 or R7)
// Returns R1 in *r1_res, stores 4 bytes in resp_buf. Returns 0 on success, -1 on error.
static int sd_cmd_r7(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t *r1_res, uint8_t resp_buf[4]) {
    *r1_res = sd_cmd(cmd, arg, crc);
    if (*r1_res == 0xFF || (*r1_res & R1_ILLEGAL_COMMAND)) {
        return -1; // Timeout or illegal command
    }
    // Read the remaining 4 bytes
    for (int i = 0; i < 4; i++) {
        resp_buf[i] = spi_transfer(0xFF);
    }
    return 0;
}


// --- Driver State ---
static struct {
  struct spinlock lock;
  int initialized;
} spi_sd_state;

// --- Public Driver Functions ---

// Initialize SPI controller and SD card
void spi_sd_init(void)
{
  initlock(&spi_sd_state.lock, "spi_sd");
  acquire(&spi_sd_state.lock);

  printf("spi_sd: initializing...\n");

  // 1. Configure SPI Controller
  write_spi_reg(SPI_CSDEF, 0xFFFFFFFF); // Default CS lines high
  write_spi_reg(SPI_CSID, SPI_CS_ID);   // Select target CS line
  write_spi_reg(SPI_CSMODE, SPI_CSMODE_AUTO); // Auto CS control initially
  write_spi_reg(SPI_SCKMODE, SPI_SCKMODE_PHA(0) | SPI_SCKMODE_POL(0)); // Mode 0 (CPOL=0, CPHA=0)
  write_spi_reg(SPI_FMT, SPI_FMT_PROTO(0) | SPI_FMT_ENDIAN(0) | SPI_FMT_DIR(0) | SPI_FMT_LEN(8)); // 8-bit, single, big-endian, tx/rx
  write_spi_reg(SPI_DELAY0, 0); // Minimal delays initially
  write_spi_reg(SPI_DELAY1, 0);
  write_spi_reg(SPI_TXMARK, 1); // Watermarks like ZSBL
  write_spi_reg(SPI_RXMARK, 0);
  write_spi_reg(SPI_IE, 0);     // Disable interrupts

  spi_set_clock(SPI_HZ_INIT);   // Set initial slow clock

  // 2. SD Card Initialization Sequence
  printf("spi_sd: sending init sequence...\n");

  // Start with CS high, send >74 clock cycles
  write_spi_reg(SPI_CSMODE, SPI_CSMODE_OFF); // Force CS high by turning controller off for this CS?
                                            // Alternative: set CSID to invalid, CSDEF handles line.
                                            // Simplest: Assume deselect puts line high.
  sd_deselect(); // Ensure CS is high initially
  delay_ms(10); // Wait > 1ms
  spi_dummy_bytes(10); // Send 80 clocks with CS high

  // Select card
  sd_select();

  // Send CMD0 (GO_IDLE_STATE)
  uint8_t r1;
  int retries = 10;
  while (retries-- > 0) {
      r1 = sd_cmd(CMD0, 0, 0x95); // CRC for CMD0 is 0x95
      if (r1 == R1_IDLE_STATE) break;
      delay_ms(10);
  }
  if (r1 != R1_IDLE_STATE) {
      panic("spi_sd_init: CMD0 failed");
  }
  printf("spi_sd: CMD0 OK (R1=0x%x)\n", r1);

  // Send CMD8 (SEND_IF_COND) - for SDv2+
  uint8_t r7_resp[4];
  int is_sdv2 = 0;
  if (sd_cmd_r7(CMD8, 0x1AA, 0x87, &r1, r7_resp) == 0 && r1 == R1_IDLE_STATE) { // CRC for CMD8 is 0x87
      // Check voltage (bit 8-11) and check pattern (bit 0-7)
      if (((r7_resp[2] & 0x0F) == 0x01) && (r7_resp[3] == 0xAA)) {
          is_sdv2 = 1;
          printf("spi_sd: CMD8 OK, SDv2 card detected.\n");
      } else {
          printf("spi_sd: CMD8 response invalid pattern/voltage.\n");
          // Should probably handle this case (maybe fall back to v1 init)
      }
  } else {
      printf("spi_sd: CMD8 failed or no response (likely SDv1/MMC).\n");
      // Assume SDv1 or MMC - init sequence differs (use CMD1 instead of ACMD41)
      // For simplicity, we proceed assuming SDv2 worked. Add v1/MMC support if needed.
  }

  // Send CMD55 + ACMD41 until card is ready
  uint32_t acmd41_arg = is_sdv2 ? (1UL << 30) : 0; // HCS bit (30) if SDv2
  retries = 1000; // Might take a while
  while (retries-- > 0) {
      // Send CMD55 (APP_CMD)
      r1 = sd_cmd(CMD55, 0, 0x65); // Dummy CRC needed
      if (r1 == R1_IDLE_STATE || r1 == 0x00) { // Should be in idle state after CMD55
          // Send ACMD41 (SD_SEND_OP_COND)
          r1 = sd_cmd(ACMD41, acmd41_arg, 0x77); // Dummy CRC
          if (r1 == 0x00) { // Card is initialized when R1 response is 0x00
             printf("spi_sd: ACMD41 OK, card initialized.\n");
             break;
          } else if (r1 != R1_IDLE_STATE) {
             printf("spi_sd: ACMD41 error response 0x%x\n", r1);
             panic("spi_sd_init: ACMD41 failed");
          }
          // else still in idle state, continue loop
      } else {
          printf("spi_sd: CMD55 failed response 0x%x\n", r1);
          // Should retry or handle MMC (CMD1)? Panic for now.
          panic("spi_sd_init: CMD55 failed");
      }
      delay_ms(1); // Wait 1ms between attempts
  }
  if (retries <= 0) {
      panic("spi_sd_init: ACMD41 timeout");
  }

  // Optional: Read OCR with CMD58 if needed (e.g., check CCS bit for block addressing)

  // Set block length to 512 (required by SDv1, ignored by SDv2/HC/XC)
  // r1 = sd_cmd(CMD16, 512, 0xFF); // Dummy CRC
  // if (r1 != 0x00) {
  //    printf("spi_sd: CMD16 failed response 0x%x (may be normal for SDv2+)\n", r1);
  //    // Allow this to fail silently for SDv2+
  // }

  // Initialization complete, increase clock speed
  spi_set_clock(SPI_HZ_FAST);

  sd_deselect(); // Deselect card

  spi_sd_state.initialized = 1;
  printf("spi_sd: initialization complete.\n");
  release(&spi_sd_state.lock);
}


// Read/Write a block (BSIZE = 1024 bytes)
// Corresponds to two 512-byte SD card blocks
void spi_sd_rw(struct buf *b, int write)
{
  if (!spi_sd_state.initialized) {
    panic("spi_sd_rw: driver not initialized");
  }
  if (b->blockno >= (FSSIZE * (BSIZE/512))) { // Check block bounds if FSSIZE is known
    // panic("spi_sd_rw: blockno (%d) out of range", b->blockno);
    // Need FSSIZE from superblock or elsewhere. Temporarily disable check.
  }

  acquire(&spi_sd_state.lock);

  uint32 lba = b->blockno * (BSIZE / 512); // Calculate base 512-byte LBA
  uint8_t *data_ptr = b->data;
  int success = 1;
  uint8_t r1;
  // uint64 start_cycle; // For timeout

  // Perform two 512-byte transfers
  for (int i = 0; i < (BSIZE / 512); i++) {
    uint32 current_lba = lba + i;
    uint8_t *current_data = data_ptr + (i * 512);

    sd_select();

    if (write) {
      // --- Write Operation ---
      r1 = sd_cmd(CMD24, current_lba, 0xFF); // WRITE_SINGLE_BLOCK
      if (r1 != 0x00) {
        printf("spi_sd_rw: CMD24 failed (LBA %d), R1=0x%x\n", current_lba, r1);
        success = 0;
        goto cleanup;
      }

      // Send data block preceded by start token
      spi_transfer(0xFF); // Dummy byte before token
      spi_transfer(START_BLOCK_TOKEN);
      for (int j = 0; j < 512; j++) {
        spi_transfer(current_data[j]);
      }
      spi_transfer(0xFF); // Dummy CRC byte 1
      spi_transfer(0xFF); // Dummy CRC byte 2

      // Check data response token
      r1 = spi_transfer(0xFF);
      if ((r1 & WRITE_ACCEPTED_MASK) != WRITE_ACCEPTED) {
        printf("spi_sd_rw: Write failed (LBA %d), Data Response=0x%x\n", current_lba, r1);
        success = 0;
        goto cleanup;
      }

      // Wait for card to finish writing (poll until not busy - reads non-0x00)
      // start_cycle = get_cycles();
      while (spi_transfer(0xFF) == 0x00) {
          // Add timeout check here if enabled
          // if (check_timeout(start_cycle, TIMEOUT_CYCLES * 2)) { // Longer timeout for write
          //    printf("spi_sd_rw: Write busy timeout (LBA %d)\n", current_lba);
          //    success = 0;
          //    goto cleanup;
          // }
      }

    } else {
      // --- Read Operation ---
      r1 = sd_cmd(CMD17, current_lba, 0xFF); // READ_SINGLE_BLOCK
      if (r1 != 0x00) {
        printf("spi_sd_rw: CMD17 failed (LBA %d), R1=0x%x\n", current_lba, r1);
        success = 0;
        goto cleanup;
      }

      // Wait for data start token (0xFE)
      // start_cycle = get_cycles();
      while ((r1 = spi_transfer(0xFF)) != START_BLOCK_TOKEN) {
        if (r1 != 0xFF) { // Check for error token instead of start
           printf("spi_sd_rw: Read failed (LBA %d), received error token 0x%x before data\n", current_lba, r1);
           success = 0;
           goto cleanup;
        }
        // Add timeout check here if enabled
        // if (check_timeout(start_cycle, TIMEOUT_CYCLES)) {
        //     printf("spi_sd_rw: Read timeout waiting for data token (LBA %d)\n", current_lba);
        //     success = 0;
        //     goto cleanup;
        // }
      }

      // Read 512 data bytes
      for (int j = 0; j < 512; j++) {
        current_data[j] = spi_transfer(0xFF);
      }

      // Read and discard 2 CRC bytes
      spi_transfer(0xFF);
      spi_transfer(0xFF);
    }

    sd_deselect(); // Deselect after each 512-byte operation

    if (!success) break; // Exit loop if one half failed
  } // End loop for two 512-byte blocks

cleanup:
  if (!success) {
    sd_deselect(); // Ensure card is deselected on error
    release(&spi_sd_state.lock);
    panic("spi_sd_rw: operation failed");
  }

  if (!write) {
      b->valid = 1; // Mark buffer as valid after successful read
  }
  // For write, buffer cache handles clearing B_DIRTY flag implicitly

  release(&spi_sd_state.lock);
}