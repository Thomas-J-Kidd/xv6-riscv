#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    printf("kinit() returned\n");
    kvminit();       // create kernel page table
    printf("kvminit() returned\n");
    kvminithart();   // turn on paging
    printf("kvminithart() returned\n");
    procinit();      // process table
    printf("procinit() returned\n");
    trapinit();      // trap vectors
    printf("trapinit() returned\n");
    trapinithart();  // install kernel trap vector
    printf("trapinithart() returned\n");
    plicinit();      // set up interrupt controller
    printf("plicinit() returned\n");
    plicinithart();  // ask PLIC for device interrupts
    printf("plicinithart() returned\n");
    binit();         // buffer cache
    printf("binit() returned\n");
    iinit();         // inode table
    printf("iinit() returned\n");
    fileinit();      // file table
    printf("fileinit() returned\n");
    // virtio_disk_init(); // emulated hard disk
    spi_sd_init(); // SD card
    printf("spi_sd_init() returned\n");
    userinit();      // first user process
    printf("userinit() returned\n");
    __sync_synchronize();
    started = 1;
    printf("CPU 0 initialization complete, started = %d\n", started);
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
