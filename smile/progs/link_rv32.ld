/* **********************************************************************
   progs/link_rv32.ld
   ********************************************************************** */
/* Sebastian Claudiusz Magierowski Oct 21 2025 */
/* 
  Minimal linker script in GNU linker script language.
  After _start and main are compiled, this will place _start at the very beginning of 
  the .text section, then main will be placed after it, along with any other code in 
  .text* sections.  This is needed so that when the program is loaded at address 0x0, 
  the CPU can start executing from _start by simply pointing PC to 0x0.  Without this, 
  the linker might place _start somewhere else, causing the CPU to jump to an incorrect 
  address on reset.  This script also defines the memory layout of the program, 
  placing code in .text, .rodata, .data, and .bss sections at specific locations.
*/
ENTRY(_start)            /* set ELF entry symbol, linker expects entry fn. to be called _start */

SECTIONS 
{
  . = 0x00000000;        /* set location counter (where next output section comes) so, places code at 0x0 */
  /* Code/text: put _start first, then everything else in .text* */
  .text : {              /* build output .text section from matching input sections   */
    KEEP(*(.text.start)) /* forces .text.start input section to be included & placed first */
    *(.text*)            /* then pulls in rest (e.g., .text, .text.main, .text.foo)   */
  }
  . = ALIGN(4);          /* keeps sections word aligned */
  .rodata : { *(.rodata*) }
  . = ALIGN(4);
  .data : { *(.data*) }
  . = ALIGN(4);
  .bss : { *(.bss*) *(COMMON) }
}