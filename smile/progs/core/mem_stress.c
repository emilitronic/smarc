// **********************************************************************
// smile/progs/core/mem_stress.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 10 2026
// Memory ordering/dependency stress for Tile1 timed data path.
//
// This kernel intentionally exercises:
// - store -> load same-address ordering
// - load-use dependency
// - SB/SH on a word-only memory port (timed read-modify-write path)

#include <stdint.h>

#define OUT_ADDR 0x0100u
#define OUT      ((volatile uint32_t*)OUT_ADDR)

__attribute__((naked, section(".text.start")))
void _start(void) {
  __asm__ volatile(
    "li   sp, 0x00004000\n"
    "j    main\n"
  );
}

static inline void exit_with_code(uint32_t code) {
  __asm__ volatile(
    "mv a0, %0\n"  // put exit "code" in a0
    "li a7, 93\n"  // Tile1's special "exit" syscall number is 93, so put that in a7
    "ecall\n"
    :
    : "r"(code)
    : "a0", "a7", "memory" // may clobber a0/a7 and may touch memory so tells compiler to be careful about optimizing around this
  );
  for (;;) {}
}

int main(void) {
  // point to unsigned 32b int at location 0x0200
  volatile uint32_t* p = (volatile uint32_t*)0x0200u; // voltatile so compiler retains every r/w through this pointer, don't optimize them out

  *p = 0u;          // a simple write for know initialization, immediately followed by…
  *p = 0x11223344u; // …another write (lane 3=0x11, lane 2=0x22, lane 1=0x33, lane 0=0x44)

  // store->load same address + load-use dependency
  uint32_t x = *p;
  uint32_t y = x + 1u;

  // Word-only memory port: SB/SH exercise timed read-modify-write behavior.
  *(volatile uint8_t*)((uintptr_t)p + 1u) = 0xAAu;       // lane 1 byte write (1 byte to 0x0201)
  uint32_t z = *p;                                       // 32b load of same word (0x0200-0x0203) 
  *(volatile uint16_t*)((uintptr_t)p + 2u) = 0xBEEFu;    // lane 2-3upper halfword write (2 bytes to 0x0202-0x0203)
  uint32_t w = *p;

  uint32_t result = y ^ z ^ w; // XOR to form a checksum
  *OUT = result;               // write checksum to 0x0100 for you to check (expect 0xBEEF3345)

  exit_with_code(result & 0xffu);
  return 0;
}
