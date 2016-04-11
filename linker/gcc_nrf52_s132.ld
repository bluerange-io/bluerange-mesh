/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x1c000, LENGTH = 0x60000
  RAM (rwx) :  ORIGIN = 0x20002e80, LENGTH = 0x5180

  BOOTLOADER (rx) : ORIGIN = 0x3c000, LENGTH = 0x4000
}

INCLUDE "nrf5x_common.ld"
