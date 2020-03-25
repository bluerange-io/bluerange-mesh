/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000, LENGTH = 0x55000  /* Length was chosen arbitrarily */

  RAM (rwx) :  ORIGIN = 0x20000000, LENGTH = 0xa1d0
}
