// **********************************************************************
// smile/progs/smurf_debug.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 2 2025

#include <stdint.h>

#define SCRATCH0_ADDR 0x0100u
#define SCRATCH1_ADDR 0x0104u

static inline void trigger_break(void) {
  __asm__ volatile("ebreak");
}

static inline void trigger_exit(uint32_t code) {
  register uint32_t a0 asm("a0") = code;
  register uint32_t a7 asm("a7") = 93;
  __asm__ volatile("ecall" : : "r"(a0), "r"(a7));
}

__attribute__((naked, section(".text.start")))
void _start(void) {
  __asm__ volatile(
    "li   sp, 0x00004000\n"
    "j    main\n"
  );
}

int main(void) {
  volatile uint32_t* scratch0 = (volatile uint32_t*)SCRATCH0_ADDR;
  volatile uint32_t* scratch1 = (volatile uint32_t*)SCRATCH1_ADDR;

  *scratch0 = 0x11112222u;
  *scratch1 = 0x33334444u;

  __asm__ volatile(
    "li t0, 0xABCDEF00\n"
    "li t1, 0x12345678\n"
    "li s0, 0xDEADBEEF\n"
    "li a0, 0x1F\n"
  );

  trigger_break();
  trigger_exit(0x2Au);

  for (;;) {
  }
  __builtin_unreachable();
}
