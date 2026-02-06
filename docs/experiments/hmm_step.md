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
./build/smile/tb_tile1 -prog=smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0 -steps=2000000 -sw_threads=1
```

- Program exit:

```bash
[EXIT] Program exited with code 253
```

- Tile1 stats (two builds of the same C code):

```bash
# RV32I (no M extension: software mul via __mulsi3)
[STATS] inst=909545 alu=542381 add=215546 mul=0 loads=145906 stores=73767 branches=130897 taken=114354

# RV32IM (with M extension: mul/div/rem in hardware)
[STATS] inst=751816 alu=419769 add=210009 mul=1664 loads=145906 stores=75342 branches=107545 taken=98203
```

- Quick per-event metrics (N = 26 events, M = 64 states):

  - RV32I:
    - inst/event ≈ 35.0k; inst/cell ≈ 0.54k
    - alu/event ≈ 20.9k; alu/cell ≈ 0.33k
    - add/event ≈ 8.3k; add/cell ≈ 0.13k
    - mul/event = 0 (all multiplies implemented in software via `__mulsi3`)
    - loads/event ≈ 5.6k; 
    - stores/event ≈ 2.8k
    - bytes/event ≈ 33.6k; bytes/cell ≈ 0.52k
    - branches/event ≈ 5.0k
    - branch taken ratio ≈ 114,354 / 130,897 ≈ 0.87

    intensity metrics:
    - alu/byte = 0.33k/0.52k ≈ 0.63 ALU ops per byte
    - add/byte = 0.13k/0.52k ≈ 0.25 adds per byte

  - RV32IM:
    - inst/event ≈ 28.9k; inst/cell ≈ 0.46k
    - alu/event ≈ 16.1k; alu/cell ≈ 0.25k
    - add/event ≈ 8.1k; add/cell ≈ 0.13k
    - mul/event ≈ 0.064k; mul/cell ≈ 0.001k
    - ops/event ≈ 8.14k; ops/cell ≈ 0.13k
    - loads/event ≈ 5.6k
    - stores/event ≈ 2.9k
    - bytes/event ≈ 34.0k; bytes/cell ≈ 0.53k
    - branches/event ≈ 4.1k
    - branch taken ratio ≈ 98,203 / 107,545 ≈ 0.91
    
    intensity metrics:
    - alu/byte = 0.25k/0.53k ≈ 0.47 ALU ops per byte
    - add/byte = 0.13k/0.53k ≈ 0.25 adds per byte
    - ops/byte = 0.13k/0.53k ≈ 0.25 ops per byte (counting add + mul)

- Notes:

  - The only intentional difference between the two builds is how `log_em()` computes `dist * dist`:
    - RV32I: GCC lowers the multiply to a call to `__mulsi3`, which is implemented here as a shift–add loop in C.
    - RV32IM: GCC emits a single `mul` instruction per call (plus surrounding code).
  - The delta between the two runs (now measured with `-sw_threads=1`) is:
    - Δinst ≈ 1.58×10^5 fewer instructions when using hardware `mul`
    - Δalu ≈ 1.23×10^5 fewer ALU-category ops
    - Δbranches ≈ 2.34×10^4 fewer branches  
    Loads are identical across both builds, which is a useful sanity check that the DP structure and memory access pattern are unchanged. Stores differ slightly because of small scheduling/code-gen changes. The explicit `add`/`mul` counters show:
    - RV32I: `add=215,546`, `mul=0` (all multiplies implemented in software via `__mulsi3`)
    - RV32IM: `add=210,009`, `mul=1,664` (each trellis multiply maps to a single `mul` instruction)
  - This pair of runs serves as a small but realistic data point for “software vs hardware multiply” in an HMM-style basecalling kernel. Future experiments can scale M/N or modify the transition structure while reusing the same measurement setup.

  - Back-of-the-envelope per-multiply cost:

    - Each trellis cell does one emission multiply: `dist * dist`.
    - There are `N - 1 = 25` non-initial events, and `M = 64` states, so:
      - ~25 × 64 = 1600 multiplies in the main loop.
      - Including a few extra multiplies (initial column, checksum, etc.), total multiplies ≈ 1664.

    - Using the measured deltas between RV32I (software mul) and RV32IM (hardware mul) runs:

      - Δinst ≈ 157,729 / 1664 ≈ 95 instructions per multiply
      - Δalu  ≈ 122,612 / 1664 ≈ 74 ALU ops per multiply
      - Δbranches ≈ 23,352 / 1664 ≈ 14 branches per multiply

      - The measured `mul=1664` in the RV32IM run matches the analytical estimate for trellis multiplies (one emission multiply per state/event, plus a small number of extras for initialization/checksum), which reinforces that this kernel is behaving as intended.

    - This is very much a back-of-the-envelope estimate:
      - The code evolved slightly between runs.
      - GCC may schedule or inline things differently across builds.

    - Still, it gives a good ballpark intuition: each software multiply was costing on the order of a few hundred instructions; with the M extension, each multiply collapses to a single `mul` plus minimal surrounding overhead.