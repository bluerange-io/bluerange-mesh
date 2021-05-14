/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x26000, LENGTH = 0xBB000

  RAM (rwx) :  ORIGIN = 0x20005D98, LENGTH = 0x3A1C8
  RAM_NO_INIT (rwx) :  ORIGIN = 0x2003FF60, LENGTH = 0xA0
}
