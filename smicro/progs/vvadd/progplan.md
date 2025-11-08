# RV64 bare‑metal vector add starter

Minimal program and build files. Writes `C[i] = A[i] + B[i]` in DRAM and drops a signature for the host harness.

---

## vectadd.c

```c
// Bare-metal RV64. No libc.
// Build: see Makefile below.

#include <stdint.h>

#define A_ADDR   0x80020000UL
#define B_ADDR   0x80024000UL
#define C_ADDR   0x80028000UL
#define SIG_ADDR 0x80010000UL

#ifndef N
#define N 1024u  // elements (uint64_t)
#endif

static volatile uint64_t* const A = (uint64_t*)A_ADDR;
static volatile uint64_t* const B = (uint64_t*)B_ADDR;
static volatile uint64_t* const C = (uint64_t*)C_ADDR;
static volatile uint64_t* const SIG = (uint64_t*)SIG_ADDR;

// Optional input init. Define INIT_INPUTS=1 at compile time to enable.
static void maybe_init_inputs(void) {
#if INIT_INPUTS
  for (uint64_t i = 0; i < N; ++i) {
    A[i] = i;          // A = 0,1,2,...
    B[i] = 2*i + 1;    // B = 1,3,5,...
  }
#endif
}

int main(void) {
  maybe_init_inputs();

  for (uint64_t i = 0; i < N; ++i) {
    uint64_t ai = A[i];
    uint64_t bi = B[i];
    C[i] = ai + bi;    // volatile prevents elision
  }

  // Simple checksum to aid host-side verification if desired
  uint64_t sum = 0;
  for (uint64_t i = 0; i < N; ++i) sum += C[i];

  *SIG = 0xfeedfaceCAFEBABEULL ^ sum; // mark completion

  // Park forever
  while (1) { __asm__ volatile ("wfi"); }
}
```

---

## crt0.S

```asm
/* Minimal startup that sets SP and calls main */
    .section .text.boot
    .globl _start
_start:
    la  sp, _stack_top
    call main
1:  j   1b
```

---

## linker.ld

```ld
/* Place everything in DRAM starting at 0x8000_0000. */
ENTRY(_start)
MEMORY {
  dram (rwx) : ORIGIN = 0x80000000, LENGTH = 16M
}
SECTIONS {
  .text : {
    *(.text.boot)
    *(.text*)
    *(.rodata*)
  } > dram

  .data : { *(.data*) } > dram
  .bss  : { *(.bss*) *(COMMON) } > dram

  . = ALIGN(16);
  _stack_top = ORIGIN(dram) + LENGTH(dram);
}
```

---

## Makefile

```make
# Toolchain prefix may be riscv64-unknown-elf or riscv64-linux-gnu depending on your setup.
CROSS ?= riscv64-unknown-elf
ARCH  ?= rv64imac
ABI   ?= lp64
CFLAGS   = -march=$(ARCH) -mabi=$(ABI) -O2 -ffreestanding -fno-builtin -Wall -Wextra -nostdlib -fno-stack-protector
LDFLAGS  = -T linker.ld -nostdlib -Wl,--gc-sections -static
ASFLAGS  = -march=$(ARCH) -mabi=$(ABI)

all: vectadd.elf

vectadd.elf: crt0.o vectadd.o linker.ld
	$(CROSS)-gcc $(LDFLAGS) -o $@ crt0.o vectadd.o

vectadd.o: vectadd.c
	$(CROSS)-gcc $(CFLAGS) -DN=$(N) $(if $(INIT_INPUTS),-DINIT_INPUTS=1,) -c -o $@ $<

crt0.o: crt0.S
	$(CROSS)-gcc $(ASFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.elf
```

---

## Usage

1. Build: `make N=1024 INIT_INPUTS=1` or just `make`.
2. In your Cascade harness: load `vectadd.elf`, set reset PC from ELF entry, run.
3. On completion, read:

   * Result vector at `C_ADDR` for `N` 64-bit words.
   * Signature at `SIG_ADDR` for quick pass/fail.

Addresses `A_ADDR`, `B_ADDR`, `C_ADDR`, and `SIG_ADDR` are literals in DRAM. They do not consume `.data`; the program treats them as memory-mapped buffers. Ensure your DRAM model covers these ranges.
