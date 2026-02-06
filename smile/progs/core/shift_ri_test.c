// **********************************************************************
// smile/progs/core/shift_ri_test.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 1 2026
/*
 * Core regression: SRLI/SRAI sanity check.
 * Exits with code 1 on success, 0 on failure.
 */
#include <stdint.h>

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
  uint32_t u = 0x80000000u;
  uint32_t l = u >> 1;              // SRLI expected: 0x40000000
  int32_t  s = (int32_t)u;
  uint32_t a = (uint32_t)(s >> 1);  // SRAI expected: 0xC0000000
  uint32_t ok = (l == 0x40000000u && a == 0xC0000000u) ? 1u : 0u;
  exit_with_code(ok);
  return 0;
}
