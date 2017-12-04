/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x23000, LENGTH = 0x30000 /* Length was chosen arbitrarily */
  RAM (rwx) :  ORIGIN = 0x20004098, LENGTH = 0xbf68
}

INCLUDE "nrf5x_common.ld"
