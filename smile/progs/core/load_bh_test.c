// **********************************************************************
// smile/progs/core/load_bh_test.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 1 2026
/*
 * Core regression: LB/LH/LBU/LHU sanity check.
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
  // Pattern bytes at 0x200: 0x80, 0x7F, 0x01, 0xFF
  *BASE_ADDR = 0xFF017F80u;

  int32_t  lb0 = 0;
  int32_t  lb1 = 0;
  uint32_t lbu0 = 0;
  int32_t  lh0 = 0;
  uint32_t lhu1 = 0;

  __asm__ volatile ("lb  %0, 0(%1)" : "=r"(lb0) : "r"(BASE_ADDR));
  __asm__ volatile ("lb  %0, 1(%1)" : "=r"(lb1) : "r"(BASE_ADDR));
  __asm__ volatile ("lbu %0, 0(%1)" : "=r"(lbu0) : "r"(BASE_ADDR));
  __asm__ volatile ("lh  %0, 0(%1)" : "=r"(lh0) : "r"(BASE_ADDR));
  __asm__ volatile ("lhu %0, 2(%1)" : "=r"(lhu1) : "r"(BASE_ADDR));

  uint32_t ok = 1u;
  if (lb0 != (int32_t)0xFFFFFF80u) ok = 0u;
  if (lb1 != 0x7Fu) ok = 0u;
  if (lbu0 != 0x80u) ok = 0u;
  if (lh0 != (int32_t)0x00007F80u) ok = 0u;
  if (lhu1 != 0x0000FF01u) ok = 0u;

  exit_with_code(ok);
  return 0;
}
