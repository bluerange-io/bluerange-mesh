/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

/* 
Total Flash 256K: 114K for s130, application starts at 0x1c000 with max size of 147K
Total RAM 32KB: 10K for S130, application can use up to 22K (data:fixed size, heap:grows upwards, stack:grows downwards)
*/

MEMORY
{
	/*APPLICATION*/
  FLASH (rx) : ORIGIN = 0x1b000, LENGTH = 0x20000
  RAM (rwx) :  ORIGIN = 0x200030F0, LENGTH = 0x4EAC
  RAM_NO_INIT (rwx) :  ORIGIN = 0x20007F9C, LENGTH = 0x0064
}
