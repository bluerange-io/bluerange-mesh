/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x26000, LENGTH = 0xBB000

  /* Length was chosen arbitrarily */
  RAM (rwx) :  ORIGIN = 0x20005d98, LENGTH = 0x3a268
}

INCLUDE "nrf5x_common.ld"
