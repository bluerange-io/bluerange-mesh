/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x26000, LENGTH = 0x2C000 /* Length was chosen arbitrarily */
  RAM (rwx) :  ORIGIN = 0x200048e0, LENGTH = 0x3b720
}

INCLUDE "nrf5x_common.ld"
