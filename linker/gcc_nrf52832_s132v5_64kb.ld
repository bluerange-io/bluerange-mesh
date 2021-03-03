/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x23000, LENGTH = 0x55000  /* Length was chosen arbitrarily */

  /* WARNING: Must use the ram allocation of the first major version of a SoftDevice. WARNING */
  /* Subsequent releases might have a smaller ram usage and the app will crash with an older SoftDevice! */
  RAM (rwx) :  ORIGIN = 0x20005e30, LENGTH = 0xa16c
  RAM_NO_INIT (rwx) :  ORIGIN = 0x2000FF9c, LENGTH = 0x0064
}
