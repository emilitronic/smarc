/* Sebastian Magierowski Oct 21 */
/* Minimal linker script */
ENTRY(_start)            /* sets the ELF entry symbol */

SECTIONS
{
  . = 0x00000000;        /* set location counter (where next output section comes) so, places code at 0x00000000 */
  /* Code/text: put _start first, then everything else in .text* */
  .text : {              /* build output .text section from matching input sections   */
    KEEP(*(.text.start)) /* forces _start input section to be included & placed first */
    *(.text*)            /* then pulls in rest (e.g., .text, .text.main, .text.foo)   */
  }
  . = ALIGN(4);          /* keeps sections word aligned */
  .rodata : { *(.rodata*) }
  . = ALIGN(4);
  .data : { *(.data*) }
  . = ALIGN(4);
  .bss : { *(.bss*) *(COMMON) }
}