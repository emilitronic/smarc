// **********************************************************************
// smile/progs/core/bitwise_imm_test.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 1 2026
/*
 * Core regression: XORI/ORI/ANDI sanity check.
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
  uint32_t v = 0x0F0F00F0u;
  uint32_t x = v ^ 0x00FF00FFu; // expected 0x0FF0000F
  uint32_t o = v | 0x00FF00FFu; // expected 0x0FFF00FF
  uint32_t a = v & 0x00FF00FFu; // expected 0x000F00F0

  uint32_t ok = 1u;
  if (x != 0x0FF0000Fu) ok = 0u;
  if (o != 0x0FFF00FFu) ok = 0u;
  if (a != 0x000F00F0u) ok = 0u;

  exit_with_code(ok);
  return 0;
}
