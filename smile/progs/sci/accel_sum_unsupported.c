// **********************************************************************
// smile/progs/sci/accel_sum_unsupported.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 21 2026
/*
Bare-metal CUSTOM-0 unsupported-verb test.
Calls accel_custom0_r(funct3=1, ...) and exits with the returned payload.
Expected with current array-sum accelerators: ACCEL_E_UNSUPPORTED (1).
*/

#include <stdint.h>

#include "accel.h"

#define ARRAY_BASE 0x00004000u
#define LEN_WORDS  16u
#define MAILBOX0   0x00000100u

__attribute__((naked, section(".text.start")))
void _start(void) {
  __asm__ volatile(
    "li   sp, 0x00004000\n"
    "j    main\n"
  );
}

static inline void sys_exit(uint32_t code) {
  __asm__ volatile(
    "mv a0, %0\n"
    "li a7, 93\n"
    "ecall\n"
    :
    : "r"(code)
    : "a0", "a7", "memory"
  );
  __builtin_unreachable();
}

int main(void) {
  volatile uint32_t* arr = (volatile uint32_t*)ARRAY_BASE; // init array at base addr 0x4000
  for (uint32_t i = 0; i < LEN_WORDS; ++i) {
    arr[i] = i + 1u;
  }

  uint32_t out = accel_custom0_r(1u, ARRAY_BASE, LEN_WORDS); // funct3=1 currently unsupported by our accel, should return ACCEL_E_UNSUPPORTED (1)
  *(volatile uint32_t*)MAILBOX0 = out;
  sys_exit(out);
}
