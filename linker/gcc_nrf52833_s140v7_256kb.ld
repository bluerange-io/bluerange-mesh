/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x27000, LENGTH = 0x59000

  RAM (rwx) :  ORIGIN = 0x20005e00, LENGTH = 0x1a200
  RAM_NO_INIT (rwx) :  ORIGIN = 0x2001FF60, LENGTH = 0xA0
}
