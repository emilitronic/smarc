// **********************************************************************
// smile/progs/core/m_extension_test.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 5 2026
/*
 * Core regression: M-extension sanity check.
 * Covers MUL/MULW/MULH/MULHU/MULHSU/DIV/DIVU/REM/REMU.
 * Exits with code 1 on success, 0 on failure.
 */
#include <stdint.h>

__attribute__((naked, section(".text.start")))
void _start(void) {
  __asm__ volatile(
    "li sp, 0x00004000\n"
    "j main\n"
  );
}

static inline void exit_with_code(uint32_t code) {
  __asm__ volatile(
    "mv a0, %0\n"
    "li a7, 93\n"
    "ecall\n"
    :
    : "r"(code)
    : "a0", "a7", "memory"
  );
  for (;;) {}
}

static inline uint32_t rv_mul(int32_t a, int32_t b) {
  uint32_t out;
  __asm__ volatile("mul %0, %1, %2" : "=r"(out) : "r"(a), "r"(b));
  return out;
}

static inline uint32_t rv_mulw(uint32_t a, uint32_t b) {
  uint32_t out;
  __asm__ volatile(
    "mv a0, %1\n"
    "mv a1, %2\n"
    ".word 0x02b5053b\n" // mulw a0, a0, a1
    "mv %0, a0\n"
    : "=r"(out)
    : "r"(a), "r"(b)
    : "a0", "a1"
  );
  return out;
}

static inline uint32_t rv_mulh(int32_t a, int32_t b) {
  uint32_t out;
  __asm__ volatile("mulh %0, %1, %2" : "=r"(out) : "r"(a), "r"(b));
  return out;
}

static inline uint32_t rv_mulhu(uint32_t a, uint32_t b) {
  uint32_t out;
  __asm__ volatile("mulhu %0, %1, %2" : "=r"(out) : "r"(a), "r"(b));
  return out;
}

static inline uint32_t rv_mulhsu(int32_t a, uint32_t b) {
  uint32_t out;
  __asm__ volatile("mulhsu %0, %1, %2" : "=r"(out) : "r"(a), "r"(b));
  return out;
}

static inline uint32_t rv_div(int32_t a, int32_t b) {
  uint32_t out;
  __asm__ volatile("div %0, %1, %2" : "=r"(out) : "r"(a), "r"(b));
  return out;
}

static inline uint32_t rv_divu(uint32_t a, uint32_t b) {
  uint32_t out;
  __asm__ volatile("divu %0, %1, %2" : "=r"(out) : "r"(a), "r"(b));
  return out;
}

static inline uint32_t rv_rem(int32_t a, int32_t b) {
  uint32_t out;
  __asm__ volatile("rem %0, %1, %2" : "=r"(out) : "r"(a), "r"(b));
  return out;
}

static inline uint32_t rv_remu(uint32_t a, uint32_t b) {
  uint32_t out;
  __asm__ volatile("remu %0, %1, %2" : "=r"(out) : "r"(a), "r"(b));
  return out;
}

int main(void) {
  uint32_t ok = 1u;

  if (rv_mul(20, -7) != 0xFFFFFF74u) ok = 0u;
  if (rv_mulw(12u, (uint32_t)-3) != 0xFFFFFFDCu) ok = 0u;
  if (rv_mulh(0x70000000, 4) != 0x00000001u) ok = 0u;
  if (rv_mulhu(0xFFFFFFFFu, 2u) != 0x00000001u) ok = 0u;
  if (rv_mulhsu(-2, 3u) != 0xFFFFFFFFu) ok = 0u;

  if ((int32_t)rv_div(20, 3) != 6) ok = 0u;
  if ((int32_t)rv_div(-20, 3) != -6) ok = 0u;
  if (rv_div(123, 0) != 0xFFFFFFFFu) ok = 0u;
  if (rv_div((int32_t)0x80000000u, -1) != 0x80000000u) ok = 0u;

  if (rv_divu(20u, 3u) != 6u) ok = 0u;
  if (rv_divu(0xFFFFFFFFu, 2u) != 0x7FFFFFFFu) ok = 0u;
  if (rv_divu(123u, 0u) != 0xFFFFFFFFu) ok = 0u;

  if ((int32_t)rv_rem(20, 3) != 2) ok = 0u;
  if ((int32_t)rv_rem(-20, 3) != -2) ok = 0u;
  if (rv_rem(123, 0) != 123u) ok = 0u;
  if (rv_rem((int32_t)0x80000000u, -1) != 0u) ok = 0u;

  if (rv_remu(20u, 3u) != 2u) ok = 0u;
  if (rv_remu(0xFFFFFFFFu, 16u) != 15u) ok = 0u;
  if (rv_remu(123u, 0u) != 123u) ok = 0u;

  exit_with_code(ok);
  return 0;
}
