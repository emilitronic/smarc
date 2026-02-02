/*
 * Core regression: SB/SH sanity check.
 * Exits with code 1 on success, 0 on failure.
 */
#include <stdint.h>

#define BASE_ADDR ((volatile uint32_t *)0x00000200)

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
  *BASE_ADDR = 0x00000000u;

  uint32_t ok = 1u;
  uint32_t addr = (uint32_t)BASE_ADDR;

  __asm__ volatile ("sb %0, 0(%1)" : : "r"(0xAAu), "r"(addr) : "memory");
  __asm__ volatile ("sb %0, 1(%1)" : : "r"(0x55u), "r"(addr) : "memory");
  __asm__ volatile ("sh %0, 2(%1)" : : "r"(0xCC33u), "r"(addr) : "memory");

  uint32_t word = *BASE_ADDR;
  if (word != 0xCC3355AAu) ok = 0u;

  exit_with_code(ok);
  return 0;
}
