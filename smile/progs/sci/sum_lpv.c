// **********************************************************************
// smile/progs/sci/sum_lpv.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 1 2026
/*
 * Scientific LPV-style sum kernel.
 * Uses LPV data at 0x00000200 and writes the sum to 0x00000100,
 * then exits via ECALL 93 with the sum as the exit code.
 */
#include <stdint.h>

#define LPV_BASE  ((volatile uint32_t *)0x00000200)
#define SUM_ADDR  ((volatile uint32_t *)0x00000100)
#define N 16

// stack setup and entry point
__attribute__((naked, section(".text.start")))
void _start(void) {
  __asm__ volatile (
    "li sp, 0x00004000\n"
    "j main\n"
  );
}

// exit via ecall 93 with given code in a0
static inline void exit_with_code(uint32_t code) {
  __asm__ volatile (
    "mv a0, %0\n"
    "li a7, 93\n"
    "ecall\n"
    :
    : "r"(code)
    : "a0", "a7", "memory"
  );
  for (;;) {}
}

// main sum kernel
int main(void) {
  uint32_t i;
  // crate your array of LPV data
  for (i = 0; i < N; ++i) {
    LPV_BASE[i] = i + 1;
  }

  // now sum it up
  uint32_t acc = 0;
  for (i = 0; i < N; ++i) {
    acc += LPV_BASE[i];
  }

  // write sum result in specified addr and exit with sum as code
  *SUM_ADDR = acc;
  exit_with_code(acc);
  return 0;
}
