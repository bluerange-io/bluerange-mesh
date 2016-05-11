/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

/* 
Total Flash 256K: 114K for s130, application starts at 0x1c000 with max size of 147K
Total RAM 32KB: 10K for S130, application can use up to 22K (data:fixed size, heap:grows upwards, stack:grows downwards)

For 16KB devices, we have ram up 0x20004000  - 1536 bytes SoftDevice callstack
*/

MEMORY
{
  FLASH (rx) : ORIGIN = 0x1c000, LENGTH = 0x24000
  RAM (rwx) :  ORIGIN = 0x20002e80, LENGTH = 0x1C00
}

INCLUDE "nrf5x_common.ld"

