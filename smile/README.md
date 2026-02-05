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

### Cross-Compile & Link
First we need to cross-compile and link our C code into a 32-b RISC-V ELF that boots at `_start` with code placed at address `0`.
```bash
# Cross-compile & link smurf.c into RV32I ELF (no M,A,F,D)
# -Os:           :optimize for size
# rv32i_zicsr    :generate RV32i instr (no HW mult/div, no atomics, no float)
# ilp32          :means ints, longs, and pointers are all 32b ABI
# freestanding   :no OS, so compiler can't expect printf, exit, startup code,..., you provide own entry stack
# nostdlib       :prevents linking in libc, libgcc, or crt0.o, 
# nostartfiles   :doesn't include standard startup files that normally include _start and call main.
# -Ttext=0x0     :pins .text (your code) starting at address 0x0, but doesn't guarantee main is 1st symbol 
# -Wl,-e,_start  :set entry point to the _start symbol (not main)
# -Wl,--no-relax :avoids linker relaxations that can assume a full runtime (keeps code sequence literal)
#  No stdlib/startfiles → So _start really is the first code executed after reset
riscv64-unknown-elf-gcc -Os -march=rv32i_zicsr -mabi=ilp32 -ffreestanding -nostdlib -nostartfiles -Wl,-Ttext=0x0 -Wl,-e,_start -Wl,--no-relax -o prog.elf smurf.c
# Alternatively, use a linker script `link_rv32.ld` to force program start at 0x0
riscv64-unknown-elf-gcc -Os -march=rv32i_zicsr -mabi=ilp32 -ffreestanding -nostdlib -nostartfiles -Wl,-T link_rv32.ld -Wl,-e,_start -Wl,--no-relax -o prog.elf smurf.c
```

### Dump a Raw Binary
Second we need to convert the ELF into a flat raw binary suitable for our simple loader (`FlatBinLoader.hpp/cpp`).  The result is concatenated loadable bytes laid out per their virtual memory addresses.  But with a stripped ELF and hence lost ELF metadata, we won't know (easily) where the code `_start` is (unless we arrange for this with some linker script).
```bash
# -O :convert the ELF’s loadable bytes into a raw, headerless blob
# Strips 32b ELF into flat raw binary, so program.bin can load at addr 0x0 and start at pc 0x0
riscv64-unknown-elf-objcopy -O binary prog.elf prog.bin
```

### Sanity Checks on ELF
If you are suspicious of your bin, you can inspect your ELF.  

#### Header check (looks at structure, but not actual code)
```bash
riscv64-unknown-elf-readelf -h prog.elf | egrep 'Class|Machine|Flags'
```
Should give something like
```bash
Class:           ELF32  # it's a 32b binary
Machine:         RISC-V # correct target
Flags:           0x0    # no special ISA flags set (e.g., no compressed “C” flag). Plain RV32I as requested
```

#### Disassembly (can append `| head` to show just the first few lines starting at 0x0)
```bash
# -d :scans executable sections & converts to annotated assembly
# -r :displays relocation entries
riscv64-unknown-elf-objdump -dr prog.elf
```
If you didn't use any linker program `*.ld`, you should get something like
```bash
Disassembly of section .text:

00000000 <main>:
   0: 10002023   sw  zero,256(zero)   # 0x100
   4: 00000793   li  a5,0
   8: 00a00693   li  a3,10
   c: 10002703   lw  a4,256(zero)     # 0x100
  10: 00f70733   add a4,a4,a5
  14: 10e02023   sw  a4,256(zero)     # 0x100
  18: 00178793   addi a5,a5,1
  1c: fed798e3   bne a5,a3, c         # loop back to 0x0c
  20: 10002703   lw  a4,256(zero)     # sum
  24: 02d00793   li  a5,45
  28: 00f71a63   bne a4,a5,3c         # if sum != 45: else path
  2c: 00100793   li  a5,1
  30: 10f02223   sw  a5,260(zero)     # 0x104 = 1
  34: 00000073   ecall
  38: 0000006f   j   38               # self-loop after halt
  3c: 000017b7   lui a5,0x1
  40: bad78793   addi a5,a5,-1107     # 0xBAD
  44: fedff06f   j   30               # store 0xBAD then ecall

00000048 <_start>:
  48: 00004137   lui sp,0x4           # sp = 0x4000
  4c: fb5ff06f   j   0                # jump to main
```
Where `main` is at `0x0` as desired and `_start` sets the stack to `0x4000` and jumps to main.  You can confirm the ELF's entry point with.
```bash
riscv64-unknown-elf-readelf -h prog.elf | grep 'Entry point'
```
Although in this simple case it is ok to start at `-start_pc=0x0`, to begin at `_start` you'd need to use `-start_pc=0x48`.  So we force `_start` to `0x0` with `link_rv32.ld`.  With such a linker you get
```bash
00000000 <_start>:
   0:	00004137   lui	sp,0x4
   4:	0040006f   j	8 <main>

00000008 <main>:
   8:	10002023   sw	zero,256(zero) # 100 <main+0xf8>
   c:	00000793   li	a5,0
  10:	00a00693   li	a3,10
  14:	10002703   lw	a4,256(zero) # 100 <main+0xf8>
  18:	00f70733   add	a4,a4,a5
  1c:	10e02023   sw	a4,256(zero) # 100 <main+0xf8>
  20:	00178793   addi	a5,a5,1
  24:	fed798e3   bne	a5,a3,14 <main+0xc>
  28:	10002703   lw	a4,256(zero) # 100 <main+0xf8>
  2c:	02d00793   li	a5,45
  30:	00f71a63   bne	a4,a5,44 <main+0x3c>
  34:	00100793   li	a5,1
  38:	10f02223   sw	a5,260(zero) # 104 <main+0xfc>
  3c:	00000073   ecall
  40:	0000006f   j	40 <main+0x38>
  44:	000017b7   lui	a5,0x1
  48:	bad78793   addi	a5,a5,-1107 # bad <main+0xba5>
  4c:	fedff06f   j	38 <main+0x30>
```

#### See sections and their addresses
```bash
riscv64-unknown-elf-readelf -W -S prog.elf | egrep 'Name|\.text|\.rodata|\.data|\.bss'
```
`.text`: all your code (_start, main, run_trellis, etc.)
`.rodata`: constants (e.g., neg_log_ptau, mu_over_stdv, event_over_stdv)
`.data`: initialized globals
`.bss`: uninitialized globals (my big arrays, e.g., log_post, cur_log_post, last_col)

```bash
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 1] .text             PROGBITS        00000000 001000 000338 00  AX  0   0  4
  [ 2] .rodata           PROGBITS        00000338 001338 0001bc 00   A  0   0  4
  [ 3] .bss              NOBITS          000004f4 0014f4 0003a8 00  WA  0   0  4
```

#### See where individual symbols live
```bash
riscv64-unknown-elf-nm -n prog.elf | egrep 'log_post|cur_log_post|pointers|last_col|_start|main'
```
to find which global is at which address
T = text (code)
B = bss (zero-initialized data)
D = data (initialized)

```bash
00000000 T _start
0000008c T main
000004f4 b log_post
0000069c b cur_log_post
0000079c b last_col
```

#### Quick "block-size" view
```bash
riscv64-unknown-elf-objdump -h prog.elf | egrep 'text|rodata|data|bss'
```
shows section sizes/addresses:
VMA (address)
LMA
size (in bytes)

```bash
  0 .text         00000338  00000000  00000000  00001000  2**2
  1 .rodata       000001bc  00000338  00000338  00001338  2**2
  2 .bss          000003a8  000004f4  000004f4  000014f4  2**2
```

#### Sanity: ELF entry + start PC
```bash
riscv64-unknown-elf-readelf -h prog.elf | grep 'Entry point'
```
Should see:   
```bash
Entry point address:               0x0
```

## Running Programs on SMile
We should then be able to run `prog.bin` as follows, from `smarc` use `./build/smile/tb_tile1` to run
```bash
# Run the default program
./build/smile/tb_tile1 -load_addr=0x0 -start_pc=0x0
# Run some program for 200 steps
tb_tile1 -prog=./smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0 -steps=200
# Show trace
tb_tile1 -prog=./smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0 -steps=200 -trace "Tile1"
# Run compiled (as prog.bin) smurf_debug.c
tb_tile1 -prog=./smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0
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
smile> source myscript.dbg
```
- Exiting a program? How an `ecall` is interpreted by the core depends on what codes it finds in standard-stipulated registers.  For example we currently have a special "exit" syscall setting.  When the core sees `ecall` it checks register x17=a7.  If a7==93, then the core understands that 'this ecall is the exit syscall' and x10=a0==<code> denotes the program's exit status (0 means clean termination !0 denotes some kind of problem).  If a7!=93, the core checks mtvec for a trap handler address and jumps there to handle the ecall as a normal trap.  See `Tile1_exec.cpp` `exec_ecall()`.
