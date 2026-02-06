// **********************************************************************
// smile/progs/core/branch_bxx_test.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 1 2026
/*
 * Core regression: BGE/BLTU/BGEU sanity check.
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
  int32_t  a = 5;
  int32_t  b = -1;
  uint32_t ua = 1u;
  uint32_t ub = 2u;
  uint32_t uc = 2u;
  uint32_t ud = 1u;

  uint32_t r_bge = 0;
  uint32_t r_bltu = 0;
  uint32_t r_bgeu = 0;

  __asm__ volatile(
    "li %[rbge], 0\n"
    "bge %[a], %[b], 1f\n"
    "j 2f\n"
    "1: li %[rbge], 1\n"
    "2:\n"
    "li %[rbltu], 0\n"
    "bltu %[ua], %[ub], 3f\n"
    "j 4f\n"
    "3: li %[rbltu], 1\n"
    "4:\n"
    "li %[rbgeu], 0\n"
    "bgeu %[uc], %[ud], 5f\n"
    "j 6f\n"
    "5: li %[rbgeu], 1\n"
    "6:\n"
    : [rbge] "=&r"(r_bge), [rbltu] "=&r"(r_bltu), [rbgeu] "=&r"(r_bgeu)
    : [a] "r"(a), [b] "r"(b), [ua] "r"(ua), [ub] "r"(ub), [uc] "r"(uc), [ud] "r"(ud)
    : "memory"
  );

  uint32_t ok = (r_bge == 1u && r_bltu == 1u && r_bgeu == 1u) ? 1u : 0u;
  exit_with_code(ok);
  return 0;
}
