// **********************************************************************
// smile/progs/smurf_threads.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 1 2025 

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
