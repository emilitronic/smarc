---
layout: default
title: hmm_step experiments
---

# HMM trellis microkernel - experiment log

Kernel: `smile/progs/sci/hmm_step.c`  
Toy workload: 3-mer HMM, M = 64 states, N = 26 events, NUM_PATH = 21 transitions/state.

Each run:

- builds `hmm_step.c` into a bare-metal RV32I ELF starting at `_start` at address `0x0`;
- converts to a flat binary `prog.bin`;
- runs on `Tile1` via `tb_tile1`;
- reports the ECALL exit code and a single stats line:
  ```bash
  [STATS] inst=... loads=... stores=... branches=... taken=...
  ```

The kernel itself:

- computes the forward trellis (Viterbi-style) with column-wise normalization,
- takes a checksum of the final column + end state,
- writes it to `0x0100`,
- exits with `result & 0xff` as the exit code.

---

## Run 2026-02-04 - baseline hmm_step.c (M=64, N=26)

- Kernel: `smile/progs/sci/hmm_step.c`
- Commit: `1b783a3`
- Host: Mac M1, riscv64-unknown-elf-gcc 13.2.0 (rv32i_zicsr, ilp32)
- Build + run commands:

```bash
# From smarc/smile/progs:
riscv64-unknown-elf-gcc -Os -march=rv32i_zicsr -mabi=ilp32 \
  -ffreestanding -nostdlib -nostartfiles \
  -Wl,-T link_rv32.ld -Wl,-e,_start -Wl,--no-relax \
  sci/hmm_step.c -o prog.elf

riscv64-unknown-elf-objcopy -O binary prog.elf prog.bin

# From smarc:
./build/smile/tb_tile1 \
  -prog=smile/progs/prog.bin \
  -load_addr=0x0 -start_pc=0x0 -steps=2000000
```

- Program exit:

```bash
[EXIT] Program exited with code 253
```

- Tile1 stats:

```bash
[STATS] inst=1819089 loads=291812 stores=147534 branches=261794 taken=228708
```

- Derived quick metrics:
  - events N = 26
  - states M = 64
  - inst/event ~= 1,819,089 / 26 ~= 69.96k
  - loads/event ~= 291,812 / 26 ~= 11.2k
  - stores/event ~= 147,534 / 26 ~= 5.7k
  - branch taken ratio ~= 228,708 / 261,794 ~= 0.87
- Notes:
  - Forward trellis only (no traceback).
  - All DP arrays are static (`.bss`); constants live in `.rodata`.
  - This is the reference point for future changes: larger M/N, different transition structure, or partial unrolling.
