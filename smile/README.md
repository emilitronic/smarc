@ -1,59 +1,166 @@
smile
=====

A sequential machines tile.

## Configure & Build
```bash
# Configure, 1st time you check out project (or when changing toolchains, compilers, or options)
# The compile argument makes smarc/build/compile_commands.json, if your VSC has CMake Tools that 
# should allow it to understand locations of files and avoid un-needed error squiggles.
/smarc $ cmake -S . -B build -DCEDAR_DIR=/Users/seb/Research/Cascade/cedar -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
# Build smile
/smarc $ cmake --build build --target tb_tile1 -j
```

# Run
```bash
# Basic
/smarc $ ./build/smile/tb_tile1
# Show the parts (and do nothing else)
./tb_tile1 -showcontexts
# Show tile's traces
./tb_tile1 -trace Tile1
# Show tile and dram
./tb_tile1 -trace "Tile1;Dram"
# Show everything
./tb_tile1 -trace "*"
```

## Making Programs for SMile
```bash
# Cross-compile & link smurf.c into RV32I ELF (no M,A,F,D), ilp32 means ints, longs, and pointers are all 32b
# freestanding tells compiler there's no OS, so it shouldn't expect printf, exit, startup code, etc., 
# nostdlib prevents linking in libc, libgcc, or crt0.o, 
# nostartfiles doesn't include standard startup files that normally include _start and call main.
# Commands to:
#  -Ttext=0x0 → Place .text (your code) starting at address 0x0000  
#  -e,_start → Entry point is the _start symbol (not main)
#  No stdlib/startfiles → So _start really is the first code executed after reset
riscv64-unknown-elf-gcc -Os -march=rv32i_zicsr -mabi=ilp32 -ffreestanding -nostdlib -nostartfiles -Wl,-Ttext=0x0 -Wl,-e,_start -Wl,--no-relax -o prog.elf smurf.c
# Alternatively, use a linker script to force program start at 0x0
riscv64-unknown-elf-gcc -Os -march=rv32i_zicsr -mabi=ilp32 -ffreestanding -nostdlib -nostartfiles -Wl,-T link_rv32.ld -Wl,-e,_start -Wl,--no-relax -o prog.elf smurf.c
# Strips 32b ELF into flat raw binary, so program.bin can load at addr 0x0 and start at pc 0x0
riscv64-unknown-elf-objcopy -O binary prog.elf prog.bin
# Header check (looks at structure, but not actual code)
riscv64-unknown-elf-readelf -h prog.elf | egrep 'Class|Machine|Flags'
# Disassembly, -d scans executable sections & converts to annotated assembly, -r displays relocation entries
riscv64-unknown-elf-objdump -dr prog.elf
```

## Running Programs on SMile
We should then be able to run `prog.bin` as follows
```bash
./build/smile/tb_tile1 -prog=./smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0 -steps=200
# Show trace
./build/smile/tb_tile1 -prog=./smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0 -steps=200 -trace "Tile1"
# Run compiled (as prog.bin) smurf_debug.c
./build/smile/tb_tile1 -prog=./smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0
# And debug the program as needed, for example
smile> help        # lists commands
smile> step 3      # advance 3 cycles
smile> regs        # dump regs and PCs
smile> mem 0x100 4 # dump 4 mem words starting at addr 0x100
smile> break 0x14  # set breakpoint at PC 0x14
smile> cont        # run until breakpoint or exit
smile> quit        # exit debugger
```

## Debugger
- set/clear breakpoints and interrogate registers and memory
- persist breakpoints between sessions
- process debugs session files enables: repeatable setup, batch/regression testing, more complex test "macros", etc.

### Notes
- By default, .smile_dbg is loaded (if present).
  - ignore_bpfile=1 gives you a “clean” session but still saves whatever breakpoints you set during the run back to .smile_dbg at exit. That means you can: Ignore old state for this run, but Still evolve the persistent file from this run onward.
- handling source <file> hook
  - leading white space is stripped
  - comments starting with \# are ignored
```bash
cat > myscript.dbg <<EOF
# simple script
break 0x14
trace on
cont
regs 0:10
quit
EOF

./build/smile/tb_tile1 -prog=... -load_addr=0x0 -start_pc=0x0 <<EOF
source myscript.dbg
EOF
# or
smile> myscript.dbg
```
