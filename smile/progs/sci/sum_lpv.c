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

__attribute__((naked, section(".text.start")))
void _start(void) {
  __asm__ volatile (
    "li sp, 0x00004000\n"
    "j main\n"
  );
}

static inline void exit_with_code(uint32_t code) {
  __asm__ volatile (
    "mv a0, %0\n" // move exit code into a0
    "li a7, 93\n" // syscall ID 93 = exit
    "ecall\n"
    // operand constraint section (what C values to make available to asm)
    :                        // no output operands
    : "r"(code)              // input operand: code in any register
    : "a0", "a7", "memory"   // declare a0 and a7 as clobbered, so compiler knows they are modified
  );
  for (;;) {} // should not reach here
}

int main(void) {
  uint32_t addr = 0x00000200u;
  uint32_t count = N;
  uint32_t val = 1;
  while (count--) {
    __asm__ volatile(
      "sw %1, 0(%0)\n"
      "addi %0, %0, 4\n"
      : "+r"(addr)
      : "r"(val)
      : "memory"
    );
    ++val;
  }

  uint32_t acc = 0;
  addr = 0x00000200u;
  count = N;
  while (count--) {
    uint32_t tmp;
    __asm__ volatile(
      "lw %0, 0(%1)\n"
      "addi %1, %1, 4\n"
      : "=r"(tmp), "+r"(addr)
      :
      : "memory"
    );
    acc += tmp;
  }

  *SUM_ADDR = acc;
  exit_with_code(acc);
  return 0;
}
