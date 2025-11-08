// **********************************************************************
// smile/progs/smexit.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 31 2025 

// Minimal flat binary that immediately issues an exit() syscall.
void _start(void) {
  register unsigned int a7 asm("a7") = 93u; // exit syscall
  register unsigned int a0 asm("a0") = 7u;  // exit code
  asm volatile(
      "ecall\n"
      :
      : "r"(a7), "r"(a0));
  for (;;) {
  }
}
