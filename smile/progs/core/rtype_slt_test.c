/*
 * Core regression: SLT/SLTU sanity check.
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
  int32_t  a = -1;
  int32_t  b = 1;
  uint32_t ua = 0xFFFFFFFFu;
  uint32_t ub = 1u;

  uint32_t slt_res = 0;
  uint32_t sltu_res = 0;

  __asm__ volatile ("slt  %0, %1, %2" : "=r"(slt_res) : "r"(a), "r"(b));
  __asm__ volatile ("sltu %0, %1, %2" : "=r"(sltu_res) : "r"(ua), "r"(ub));

  uint32_t ok = (slt_res == 1u && sltu_res == 0u) ? 1u : 0u;
  exit_with_code(ok);
  return 0;
}
