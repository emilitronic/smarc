// **********************************************************************
// smile/progs/sci/threshold_lpv.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 2 2026
/*
 * Threshold-and-count LPV-style kernel for smile.
 * Initializes LPV data at 0x00000200, counts values > THRESH,
 * writes the count to 0x00000104, and exits with that count.
 *
 * Memory sketch (not to scale):
 * 0x0000  code (.text/.rodata)
 * 0x????  _start (depends on image size)
 * 0x0100  SUM_ADDR (used by other kernels; beware overlap if code grows)
 * 0x0104  COUNT_ADDR (this kernel's result)
 * 0x0200  LPV_BASE (array data)
 * 0x4000  stack top (sp)
 */
#include <stdint.h>

#define LPV_BASE   ((volatile uint32_t *)0x00000200) // base addr of LPV data
#define COUNT_ADDR ((volatile uint32_t *)0x00000104)
#define N 16
#define THRESH 8

__attribute__((naked, section(".text.start")))
void _start(void) {
  __asm__ volatile (
    "li sp, 0x00004000\n"
    "j main\n"
  );
}

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

int main(void) {
  uint32_t i;
  // create your array of LPV data
  for (i = 0; i < N; ++i) {
    LPV_BASE[i] = i + 1;
  }

  // count how many values exceed THRESH
  uint32_t count = 0;
  for (i = 0; i < N; ++i) {
    if (LPV_BASE[i] > THRESH) {
      ++count;
    }
  }
  
  // write count result in specified addr and exit with count as code
  *COUNT_ADDR = count;
  exit_with_code(count);
  return 0;
}
