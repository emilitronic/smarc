// **********************************************************************
// smile/progs/core/smurf_threads.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 1 2025 
/*
Minimal freestanding RV32I, no libc stuff.  Works as a flat .bin
Creates two simple "threads" that increment a shared variable `sum`:
- thread0 adds 1 to sum five times, breaking after each add
- thread1 adds 2 to sum five times, breaking after each add
After both threads complete, the program ECALLs to exit, returning `sum` as the exit code.
*/
#include <stdint.h>

volatile unsigned int sum = 0;

static void thread0(void) {
  for (int i = 0; i < 5; ++i) {
    sum += 1u;
    asm volatile("ebreak");
  }
}

static void thread1(void) {
  for (int i = 0; i < 5; ++i) {
    sum += 2u;
    asm volatile("ebreak");
  }
}

void _start(void) {
  thread0();
  thread1();
  register unsigned int a7 asm("a7") = 93u;
  register unsigned int a0 asm("a0") = sum;
  asm volatile("ecall" : : "r"(a7), "r"(a0));
  for (;;) {
  }
}
