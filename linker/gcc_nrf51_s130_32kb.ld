OUTPUT_FORMAT ("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")

GROUP(-lgcc -lc -lnosys -lCMSIS)

/* 
Total Flash 256K: 114K for s130, application starts at 0x1c000 with max size of 147K
Total RAM 32KB: 10K for S130, application can use up to 22K (data:fixed size, heap:grows upwards, stack:grows downwards)
*/

MEMORY
{
  FLASH (rx) : ORIGIN = 0x1b000, LENGTH = 0x20000
  RAM (rwx) :  ORIGIN = 0x20002E80, LENGTH = 0x5180

  BOOTLOADER (rx) : ORIGIN = 0x3c000, LENGTH = 0x4000
}

INCLUDE "gcc_arm_flash.ld"

