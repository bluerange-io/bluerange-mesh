/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x26000, LENGTH = 0x55000  /* Length was chosen arbitrarily */

  /* WARNING: Must use the ram allocation of the first major version of a SoftDevice. WARNING */
  /* Subsequent releases might have a smaller ram usage and the app will crash with an older SoftDevice! */
  /* These values were determined with S132 7.0.1 */
  RAM (rwx) :  ORIGIN = 0x20005DF0, LENGTH = 0xA160
  RAM_NO_INIT (rwx) :  ORIGIN = 0x2000FF60, LENGTH = 0xA0
}
