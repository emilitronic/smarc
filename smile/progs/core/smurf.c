// **********************************************************************
// smile/progs/core/smurf.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 14 2025 
/*
Minimal freestanding RV32I, no libc stuff.  Works as a flat .bin
1) Does SUM(0…9)=45;
2_ writes SUM=45 at 0x100;
3) if sum==45 sets FLAG=1 at 0x104; 
4) then EBREAKs causing trap handler to write 0xBEEF at 0x0108 and ret to main
5) then ECALLs causing trap handler to write 0xDEAD at 0x0104 so tile sim (which looks for this) can halt.

smurf.c makes ecall invoke a trap handler. Rather than using a special "exit" syscall (a7=93).
(As coded in Tile1_exec.cpp exec_ecall()).  smurf.c writes trap handler's address into mtvec
CSR (Tile1::write_csr() deals with this write).  smurf.c also defines the trap handler itself.
Tile1's trap handling logic (Tile1::raise_trap()) jumps to the address in mtvec on a trap and
executes the handler.

Create the following memory distribution:
0x0000  --------- <- CODE (.text, .rodata): start of our text (as per our compile instruction)
       |         |
0x????  --------- <- our entry point called _start, location depends on size of program
       |         |
       |         |   DATA (stratch, .data):
0x0100  --------- <- SUM_ADDR (where a result will go, clearly this is dangerous in case our program above is too big)
0x0104  --------- <- FLAG_ADDR (where another result will go, also dangerous as noted above)
       |         |
       |         |
0x4000  --------- <- STACK TOP: stack ptr (sp) (so, our stack better not go all they way down to 0x0104)
*/
#include <stdint.h> // gives fixed-width int types like uint32_t

#define SUM_ADDR_VALUE        0x0100u
#define FLAG_ADDR_VALUE       0x0104u
#define BREAK_FLAG_ADDR_VALUE 0x0108u
#define SUM_ADDR  ((volatile uint32_t*)SUM_ADDR_VALUE)  // assign name to mem addr, you'll be able to read/write from/to this addr by the name
#define FLAG_ADDR ((volatile uint32_t*)FLAG_ADDR_VALUE) // assign name to mem addr, voltatile prevents compiler from optimizing this out
#define BREAK_FLAG_ADDR ((volatile uint32_t*)BREAK_FLAG_ADDR_VALUE)
#define TRAP_FLAG_VALUE       0xDEADu
#define BREAK_FLAG_VALUE      0xBEEFu

// Correct CSR setup
static inline void write_mtvec(void (*handler)(void)) { // takes pointer to a fn (handler)
  __asm__ volatile("csrw mtvec, %0" :: "r"(handler));   // writes given addr into mtvec
}

// Tag your _start function so it lands in .text.start and thus becomes the entry point
/* Make entry point fn., (linker & runtime expect it to be named _start), for baremetal program 
  (where we go to after reset, need this if we don't use OS) naked attribute tells compiler not 
  to generate any prologue/epilogue (we're responsible for setting up register state, stack ptr, 
  etc.) 
  TODO: to grow _start towards a full crt0 that sets up data/bss sections, etc. you'll want to add
  basics like zeroing .bss, copying .data, setting gp, installing mtvec, calling C++ constructors, 
  passing args, handling return, etc.  For now, keep it minimal.
*/
__attribute__((naked, section(".text.start"))) 
void _start(void) {           // declare _start, a function that takes no arguments and returns nothing
  __asm__ volatile(
    "li   sp, 0x00004000\n"   // set stack ptr to addr 0x4000, you have given yourself a 16 KiB stack
    "j    main\n"             // after _start and main are compiled, a linker will fill in the correct addr for main
  );
} 

static inline void do_ecall(void) {
  __asm__ volatile ("ecall"); // ultimately handled by
}

static inline void do_ebreak(void) {
  __asm__ volatile ("ebreak");
}
// Trap handler: differentiate between breakpoint and ecall traps using mcause.
__attribute__((naked, section(".text.trap"))) // ensure handler is in own section (.text.trap)
void trap_handler(void) {
  __asm__ volatile(
    "addi sp, sp, -24\n"
    "sw   t0, 0(sp)\n"   // spill t0
    "sw   t1, 4(sp)\n"   // spill t1
    "sw   t2, 8(sp)\n"   // spill t2
    "csrr t0, mcause\n"  // read mcause
    "li   t1, 3\n"
    "beq  t0, t1, 1f\n"  // if mcause=3  (break) goto 1
    "li   t1, 11\n"      // else try 11
    "beq  t0, t1, 2f\n"  // if mcause=11 (ecall) goto 2
    "j    4f\n"          // else goto 4
  "1:\n"                 // on break write 0xBEEF to 0x108 goto 3
    "li   t0, %[break_addr]\n"
    "li   t1, %[break_val]\n"
    "sw   t1, 0(t0)\n"
    "csrr t0, mepc\n"
    "addi t0, t0, 4\n"
    "csrw mepc, t0\n"    // mepc+4 (for break)
    "j    3f\n"
  "2:\n"                 // on ecall write 0xDEAD to 0x104 goto 5
    "li   t0, %[ecall_addr]\n"
    "li   t1, %[ecall_val]\n"
    "sw   t1, 0(t0)\n"
    "j    5f\n"
  "3:\n"                 // mret from break at 1 (pop registers)
    "lw   t0, 0(sp)\n"
    "lw   t1, 4(sp)\n"
    "lw   t2, 8(sp)\n"
    "addi sp, sp, 24\n"
    "mret\n"
  "4:\n"                 // mret from something other than ebreak or ecall
    "lw   t0, 0(sp)\n"
    "lw   t1, 4(sp)\n"
    "lw   t2, 8(sp)\n"
    "addi sp, sp, 24\n"
    "mret\n"
  "5:\n"                 // from ecall at 2 spin (TB will notice this and terminate) 
    "lw   t0, 0(sp)\n"
    "lw   t1, 4(sp)\n"
    "lw   t2, 8(sp)\n"
    "addi sp, sp, 24\n"
  "6:\n"
    "j    6b\n"
    // operand constraint section: tells compiler what C values to make available to in-line assembly
    :                                          // output operands: none 
    : [break_addr] "i"(BREAK_FLAG_ADDR_VALUE), // input operands: what's what in C, all are "i"mmediate
      [break_val] "i"(BREAK_FLAG_VALUE),
      [ecall_addr] "i"(FLAG_ADDR_VALUE),
      [ecall_val] "i"(TRAP_FLAG_VALUE)
  );
}

int main(void) {
  volatile uint32_t *sum       = SUM_ADDR;        // sum  = addr 0x100
  volatile uint32_t *flag      = FLAG_ADDR;       // flag = addr 0x104 
  volatile uint32_t *breakflag = BREAK_FLAG_ADDR; // flag set by breakpoint trap

  write_mtvec(trap_handler);           // install trap handler into mtvec (i.e., write fn addr in m/c trap vector addr)

  *sum = 0;                            // M[0x100] <-- 0
  for (uint32_t i = 0; i < 10; ++i) {
    *sum = *sum + i;                   // exercises LW/SW and ALU in the loop, M[0x100] <-- M[0x100] + 1
  }

  if (*sum == 45u) {                   // exercises a conditional branch
    *flag = 1u;                        // M[0x104] <-- 1
  } else {
    *flag = 0xBADu;                    // M[0x104] <-- 0xBAD
  }

  (void)breakflag;                     // suppress unused warning until we poll it
  do_ebreak();                         // trigger breakpoint trap first (and come back)
  do_ecall();                          // trigger ecall trap next (and never come back)
  for(;;) {}                           // safety (won’t run if ECALL halts)
}
