// **********************************************************************
// smile/progs/core/smexit.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 31 2025 
/*
Minimal flat binary that immediately issues an exit() syscall.
Just a simple test program for SMile tile simulator to exit cleanly. 
*/
void _start(void) {
  register unsigned int a7 asm("a7") = 93u; // exit syscall (put 93 in a7)
  register unsigned int a0 asm("a0") = 7u;  // exit code (put 7 in a0, our desired exit code)
  asm volatile(
      "ecall\n"
      :
      : "r"(a7), "r"(a0));
  for (;;) {
  }
}
