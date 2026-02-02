# core programs

This directory holds core C programs used to exercise the micro-architecture,
debugger, trap plumbing, and other baseline system behaviors.

Regression tests (exit code 1 on pass, 0 on fail):
- shift_ri_test.c: SRLI/SRAI sanity check
- bitwise_imm_test.c: XORI/ORI/ANDI sanity check
- rtype_logic_test.c: SUB/XOR/OR/AND sanity check
- rtype_slt_test.c: SLT/SLTU sanity check

Build/run snippet (from repo root, replace <test>.c):
```
cd smile/progs
riscv64-unknown-elf-gcc -Os -march=rv32i_zicsr -mabi=ilp32 \
  -ffreestanding -nostdlib -nostartfiles \
  -Wl,-T link_rv32.ld -Wl,-e,_start -Wl,--no-relax \
  core/<test>.c -o prog.elf
riscv64-unknown-elf-objcopy -O binary prog.elf prog.bin
cd ../..
./build/smile/tb_tile1 -prog=smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0 -ignore_bpfile=1
```
