// **********************************************************************
// smile/progs/core/fence_test.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 1 2026
/*
 * Core regression: FENCE/FENCE.I sanity check.
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
  __asm__ volatile ("fence" ::: "memory");
  // Emit fence.i encoding directly to avoid toolchain requiring Zifencei.
  __asm__ volatile (".word 0x0000100f" ::: "memory");
  exit_with_code(1u);
  return 0;
}
