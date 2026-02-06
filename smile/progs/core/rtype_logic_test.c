// **********************************************************************
// smile/progs/core/rtype_logic_test.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 1 2026
/*
 * Core regression: SUB/XOR/OR/AND sanity check.
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
  volatile uint32_t a = 0x0F0F00F0u;
  volatile uint32_t b = 0x00FF00FFu;

  uint32_t sub = a - b;          // expect 0x0E0FFFF1
  uint32_t x   = a ^ b;          // expect 0x0FF0000F
  uint32_t o   = a | b;          // expect 0x0FFF00FF
  uint32_t n   = a & b;          // expect 0x000F00F0

  uint32_t ok = 1u;
  if (sub != 0x0E0FFFF1u) ok = 0u;
  if (x   != 0x0FF0000Fu) ok = 0u;
  if (o   != 0x0FFF00FFu) ok = 0u;
  if (n   != 0x000F00F0u) ok = 0u;

  exit_with_code(ok);
  return 0;
}
