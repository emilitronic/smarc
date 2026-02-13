---
layout: default
title: kernels
---

### HMM trellis microkernel (`hmm_step.c`)

- Location: `smile/progs/sci/hmm_step.c`
- Workload: Viterbi-style forward trellis for a 3-mer HMM
  - M = 64 states, N = 26 events, NUM_PATH = 21 transitions/state
- Result:
  - Computes a checksum over the final column + end-state
  - Stores it at `0x0100` and exits with code `result & 0xff`

```bash
# A manual and basic run of the HMM kernel.
smarc $ cd smile/progs
smarc/smile/progs $ riscv64-unknown-elf-gcc -Os -march=rv32i_zicsr -mabi=ilp32 -ffreestanding -nostdlib -nostartfiles -Wl,-T link_rv32.ld -Wl,-e,_start -Wl,--no-relax sci/hmm_step.c -o prog.elf
smarc/smile/progs $ riscv64-unknown-elf-objcopy -O binary prog.elf prog.bin
smarc/smile/progs $ cd ../..
smarc $ ./build/smile/tb_tile1 -prog=smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0 -steps=2000000
```

You should see something like
```bash
[EXIT] Program exited with code 253
[STATS] inst=1819089 loads=291812 stores=147534 branches=261794 taken=228708
```
There are some alternative ways to get this run.  First you can create the binary much more easily by building the `hmm_step` target in CMake:
```bash
# This makes smile/progs/hmm_step.bin for you (among other things)
smarc $ cmake --build build --target smile_progs -j
```
You can run the compiled program in ideal mode.

```bash
# 1) Default run (timed mode, default mem latency)
smarc $ ./build/smile/tb_tile1 -prog=smile/progs/hmm_step.bin -load_addr=0x0 -start_pc=0x0 -ignore_bpfile=1 -steps=20000000
[EXIT] Program exited with code 253
[EXIT] Program exited with code 253
[STATS] cycles=2066108 inst=924030 alu=568503 add=228239 mul=0 loads=144306 stores=73742 branches=120885 taken=109879
```

Expected:
- Program exits cleanly with code `253`.
- You should see `[EXIT] Program exited with code 253`.
- `inst`, `loads`, `stores`, and `branches` are deterministic for a fixed binary.

```bash
# 2) Explicit ideal mode (boolean flag)
smarc $ ./build/smile/tb_tile1 -prog=smile/progs/hmm_step.bin -load_addr=0x0 -start_pc=0x0 -ignore_bpfile=1 -steps=20000000 -ideal_mem=true
[EXIT] Program exited with code 253
[EXIT] Program exited with code 253
[STATS] cycles=924030 inst=924030 alu=568503 add=228239 mul=0 loads=144306 stores=73742 branches=120885 taken=109879
```

```bash
# 3) Explicit ideal mode (string flag; equivalent to -ideal_mem=true)
smarc $ ./build/smile/tb_tile1 -prog=smile/progs/hmm_step.bin -load_addr=0x0 -start_pc=0x0 -ignore_bpfile=1 -steps=20000000 -mem_model=ideal
[EXIT] Program exited with code 253
[EXIT] Program exited with code 253
[STATS] cycles=924030 inst=924030 alu=568503 add=228239 mul=0 loads=144306 stores=73742 branches=120885 taken=109879
```

Expected for both ideal runs:
- Same functional result as timed mode (`exit code 253`).
- Same instruction mix counters (`inst/loads/stores/branches`) as timed mode.
- Lower cycle count when memory latency is non-zero.

```bash
# 4) Timed vs ideal comparison at non-zero latency
smarc $ ./build/smile/tb_tile1 -prog=smile/progs/hmm_step.bin -load_addr=0x0 -start_pc=0x0 -ignore_bpfile=1 -mem_latency=3 -steps=20000000 -mem_model=timed
smarc $ ./build/smile/tb_tile1 -prog=smile/progs/hmm_step.bin -load_addr=0x0 -start_pc=0x0 -ignore_bpfile=1 -mem_latency=3 -steps=20000000 -mem_model=ideal
```

Expected (reference numbers from this repo as of Feb 2026):
```bash
# timed, mem_latency=3
[EXIT] Program exited with code 253
[STATS] cycles=4350264 inst=924030 alu=568503 add=228239 mul=0 loads=144306 stores=73742 branches=120885 taken=109879

# ideal, mem_latency=3
[EXIT] Program exited with code 253
[STATS] cycles=924030 inst=924030 alu=568503 add=228239 mul=0 loads=144306 stores=73742 branches=120885 taken=109879
```

```bash
# 5) Optional trace-heavy run for debugging
smarc $ ./build/smile/tb_tile1 -prog=smile/progs/hmm_step.bin -load_addr=0x0 -start_pc=0x0 -ignore_bpfile=1 -mem_latency=3 -steps=200 -trace "Tile1"
```

Expected:
- Per-cycle instruction trace lines from `Tile1`.
- Useful for spot-checking fetch/decode/PC flow; not recommended for full kernel runs due to log volume.


For a chronological list of runs and stats, see [hmm_step experiment log](experiments/hmm_step.md).
