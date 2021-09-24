/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x27000, LENGTH = 0x59000

  RAM (rwx) :  ORIGIN = 0x20002ae8, LENGTH = 0x1d478
  RAM_NO_INIT (rwx) :  ORIGIN = 0x2001FF60, LENGTH = 0xA0
}
