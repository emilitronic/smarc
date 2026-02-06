---
layout: default
title: hmm_step experiments
---

# HMM trellis microkernel - experiment log

- Kernel: `smile/progs/sci/hmm_step.c`
- Commit: `1b783a3`
- Host: Mac M1, riscv64-unknown-elf-gcc 13.2.0 (rv32i_zicsr, ilp32)
- Build + run commands:
Toy workload: 3-mer HMM, M = 64 states, N = 26 events, NUM_PATH = 21 transitions/state.

Each run:

- builds `hmm_step.c` into a bare-metal RV32 ELF starting at `_start` at address `0x0`;
- converts to a flat binary `prog.bin`;
- runs on `Tile1` via `tb_tile1`;
- reports the ECALL exit code and a single stats line:
  ```bash
  [STATS] inst=... alu=... loads=... stores=... branches=... taken=...
  ```

The kernel itself:

- computes the forward trellis (Viterbi-style) with column-wise normalization,
- takes a checksum of the final column + end state,
- writes it to `0x0100`,
- exits with `result & 0xff` as the exit code.

---

## Run 2026-02-04 – baseline `hmm_step.c` (M = 64, N = 26)

- Kernel: `smile/progs/sci/hmm_step.c`
- Host: Mac M1, riscv64-unknown-elf-gcc 13.2.0
- Build + run commands:

```bash
# From smarc/smile/progs:
# Build the program as RV32I (software multiply via __mulsi3)…
riscv64-unknown-elf-gcc -Os -march=rv32i_zicsr -mabi=ilp32 \
  -ffreestanding -nostdlib -nostartfiles \
  -Wl,-T link_rv32.ld -Wl,-e,_start -Wl,--no-relax \
  sci/hmm_step.c -o prog.elf

# Or, build as RV32IM (hardware M extension)…
riscv64-unknown-elf-gcc -Os -march=rv32im_zicsr -mabi=ilp32 \
  -ffreestanding -nostdlib -nostartfiles \
  -Wl,-T link_rv32.ld -Wl,-e,_start -Wl,--no-relax \
  sci/hmm_step.c -o prog.elf

# Convert to flat binary
riscv64-unknown-elf-objcopy -O binary prog.elf prog.bin

# From smarc:
./build/smile/tb_tile1 -prog=smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0 -steps=2000000
```

- Program exit:

```bash
[EXIT] Program exited with code 253
```

- Tile1 stats (two builds of the same C code):

```bash
# RV32I (no M extension: software mul via __mulsi3)
[STATS] inst=1819089 alu=1084762 loads=291812 stores=147534 branches=261794 taken=228708

# RV32IM (with M extension: mul/div/rem in hardware)
[STATS] inst=1503631 alu=839538 loads=291812 stores=150684 branches=215090 taken=196406
```

- Quick per-event metrics (N = 26 events, M = 64 states):

  - RV32I:
    - inst/event ≈ 69.9k
    - alu/event ≈ 41.7k
    - loads/event ≈ 11.2k
    - stores/event ≈ 5.7k
    - branches/event ≈ 10.1k
    - branch taken ratio ≈ 228,708 / 261,794 ≈ 0.87

  - RV32IM:
    - inst/event ≈ 57.8k
    - alu/event ≈ 32.3k
    - loads/event ≈ 11.2k
    - stores/event ≈ 5.8k
    - branches/event ≈ 8.3k
    - branch taken ratio ≈ 196,406 / 215,090 ≈ 0.91

- Notes:

  - The only intentional difference between the two builds is how `log_em()` computes `dist * dist`:
    - RV32I: GCC lowers the multiply to a call to `__mulsi3`, which is implemented here as a shift–add loop in C.
    - RV32IM: GCC emits a single `mul` instruction per call (plus surrounding code).
  - The delta between the two runs (~3.2e5 fewer instructions and ~2.5e5 fewer ALU ops when using `mul`) is consistent with replacing ~1664 software multiplies with single-cycle hardware multiplies (on the order of hundreds of instructions saved per multiply, including the call/return overhead and loop body).
  - Loads are identical across both builds, which is a useful sanity check that the DP structure and memory access pattern are unchanged.
  - This pair of runs serves as a small but realistic data point for “software vs hardware multiply” in an HMM-style basecalling kernel. Future experiments can scale M/N or modify the transition structure while reusing the same measurement setup.

  - Back-of-the-envelope per-multiply cost:

    - Each trellis cell does one emission multiply: `dist * dist`.
    - There are `N - 1 = 25` non-initial events, and `M = 64` states, so:
      - ~25 × 64 = 1600 multiplies in the main loop.
      - Including a few extra multiplies (initial column, checksum, etc.), total multiplies ≈ 1664.

    - Using the measured deltas between RV32I (software mul) and RV32IM (hardware mul) runs:

      - Δinst ≈ 315,458 / 1664 ≈ 190 instructions per multiply
      - Δalu  ≈ 245,224 / 1664 ≈ 147 ALU ops per multiply
      - Δbranches ≈ 46,704 / 1664 ≈ 28 branches per multiply

    - This is very much a back-of-the-envelope estimate:
      - The code evolved slightly between runs.
      - GCC may schedule or inline things differently across builds.

    - Still, it gives a good ballpark intuition: each software multiply was costing on the order of a few hundred instructions; with the M extension, each multiply collapses to a single `mul` plus minimal surrounding overhead.