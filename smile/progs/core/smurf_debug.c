// **********************************************************************
// smile/progs/core/smurf_debug.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 2 2025
/*
Debugger-focused test program for SMile.  Simple state patterns to spot so its easy to verify whther we have a working debugger REPL.

Sequence:
1. `_start` sets up a small stack at 0x4000 and jumps into `main`.
2. `main` initializes two “scratch” memory locations at 0x0100 and 0x0104 with known patterns.
3. It then loads recognizable constants into a few registers (t0, t1, s0, a0).
4. It executes an `ebreak`, giving the debugger a natural place to stop so you can inspect
   registers and memory via the REPL.
5. After you continue from the breakpoint, it issues an `ecall` with a7=93 and a0=0x2A so
   the core takes the “exit” path and halts with exit code 42.

Use this program to:
- exercise the debugger REPL (step, regs, mem, break/cont),
- verify that the scratch memory locations hold the expected values at the breakpoint,
- confirm that the exit syscall (93) and exit code (0x2A) are being reported correctly.
*/
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

// set up small stack and jump to main
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

  *scratch0 = 0x11112222u; // easy val in mem for debugger to spot
  *scratch1 = 0x33334444u; // easy val in mem for debugger to spot

  // load recognizable constants into some registers
  __asm__ volatile(
    "li t0, 0xABCDEF00\n"
    "li t1, 0x12345678\n"
    "li s0, 0xDEADBEEF\n"
    "li a0, 0x1F\n"
  );

  trigger_break(); // debugger regs, mem 0x100 should see state set by this command
  trigger_exit(0x2Au); // debugger should report exit code 42 (0x2A)

  for (;;) {
  }
  __builtin_unreachable();
}
