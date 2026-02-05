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
- Typical run:
```bash
cd smile/progs
smarc/smile/progs $ riscv64-unknown-elf-gcc -Os -march=rv32i_zicsr -mabi=ilp32 -ffreestanding -nostdlib -nostartfiles -Wl,-T link_rv32.ld -Wl,-e,_start -Wl,--no-relax sci/hmm_step.c -o prog.elf

smarc/smile/progs$ riscv64-unknown-elf-objcopy -O binary prog.elf prog.bin

cd ../..
smarc $ ./build/smile/tb_tile1 -prog=smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0 -steps=2000000
  ```
You should see something like
```bash
[EXIT] Program exited with code 253
[STATS] inst=1819089 loads=291812 stores=147534 branches=261794 taken=228708
```