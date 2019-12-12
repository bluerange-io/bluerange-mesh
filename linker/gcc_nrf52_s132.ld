/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x23000, LENGTH = 0x55000  /* Length was chosen arbitrarily */
  RAM (rwx) :  ORIGIN = 0x20005b88, LENGTH = 0xa478
}
INCLUDE "nrf5x_common.ld"
