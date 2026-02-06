// **********************************************************************
// smile/progs/core/rtype_shift_test.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 1 2026
/*
 * Core regression: SLL/SRL/SRA sanity check.
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
  uint32_t v = 0x80000001u;
  uint32_t sh = 1u;
  uint32_t sll = 0;
  uint32_t srl = 0;
  uint32_t sra = 0;

  __asm__ volatile ("sll %0, %1, %2" : "=r"(sll) : "r"(v), "r"(sh));
  __asm__ volatile ("srl %0, %1, %2" : "=r"(srl) : "r"(v), "r"(sh));
  __asm__ volatile ("sra %0, %1, %2" : "=r"(sra) : "r"(v), "r"(sh));

  uint32_t ok = 1u;
  if (sll != 0x00000002u) ok = 0u;
  if (srl != 0x40000000u) ok = 0u;
  if (sra != 0xC0000000u) ok = 0u;

  exit_with_code(ok);
  return 0;
}
