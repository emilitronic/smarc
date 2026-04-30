---
layout: default
title: smile
---

# smile – Tile1 core + debugger playground

`smile` is a small environment for experimenting with a single RV32 core (`Tile1`), simple accelerators, and a debugger, **outside** of the full `smicro` SoC harness.

It gives you:

- A standalone RV32 core (`Tile1`)
- A clean memory abstraction (`MemoryPort`)
- A tiny instruction decoder and execute helpers
- A debugger REPL (breakpoints, stepping, memory dump)
- A testbench (`tb_tile1`) that ties it all together

The idea: you can iterate on the core, accelerators, and debugger in `smile`, then reuse the same pieces when the core is embedded in `smicro`.

---

## Big picture: who talks to whom?

In the `tb_tile1` testbench, the wiring looks like this:

```text
         +---------------------+
         |    tb_tile1.cpp     |
         |---------------------|
         |  - creates…         |
         |    - Tile1          |
         |    - Dram           |
         |    - DramMemoryPort |
         |    - accel          |
         |    - debugger       |
         +----------+----------+
                    |
                    V
         Tile1 (CPU core, Tile1.cpp / Tile1_exec.cpp)
                    |
                    | MemoryPort::read32 / write32
                    V
       smem::DramMemoryPort (shared shim)
                    |
                    | Dram::read / Dram::write
                    V
          Dram (shared smem DRAM model)

          +-------------------+
          | AccelArraySum     |
          | (AccelArraySum.*) |
          | talks to memory   |
          | via same          |
          | MemoryPort        |
          +-------------------+
```
On each cycle:
1. The debugger calls Sim::run().
2.	Cascade ticks the clock and calls Tile1::tick().
3.	Tile1::tick() fetches an instruction via mem_port_->read32(pc).
4.	The memory port shim turns that into a Dram::read(...) and returns the word.
5.	Tile1 decodes and executes the instruction (possibly calling the accelerator).
6.	The debugger inspects state and prints traces / breakpoints as needed.

## smile/ directory structure


```bash
smile/
├── include/                # Public headers for core, memory shim, accelerators, debugger
│   ├── AccelArraySum.hpp     # single-cycle array-sum accelerator interface
│   ├── AccelArraySumMc.hpp   # multi-cycle array-sum accelerator interface
│   ├── AccelDemoAdd.hpp      # demo add accelerator interface
│   ├── AccelPort.hpp         # abstract accelerator contract + ACCEL_E_* codes
│   ├── Debugger.hpp          # debugger REPL interface
│   ├── Diagnostics.hpp       # postmortem diagnostics helpers
│   ├── Instruction.hpp       # RV32 decoder interface
│   ├── Tile1_exec.hpp        # exec_* helper declarations
│   └── Tile1.hpp             # Tile1 core interface
├── src/
│   ├── AccelArraySum.cpp     # single-cycle array-sum accelerator
│   ├── AccelArraySumMc.cpp   # multi-cycle array-sum accelerator
│   ├── AccelDemoAdd.cpp      # trivial demo accelerator
│   ├── Debugger.cpp          # debugger REPL + stepping logic
│   ├── Diagnostics.cpp       # helper traces/asserts
│   ├── Instruction.cpp       # RV32 decoder
│   ├── tb_tile1.cpp          # testbench main() + suite injection
│   ├── Tile1_exec.cpp        # exec_* helpers (ALU, load/store, branch, CSR, custom0)
│   ├── Tile1.cpp             # core fetch/decode/execute/trap + stall logic
│   └── util/
│       ├── FlatBinLoader.cpp # load flat .bin into MemoryPort
│       └── FlatBinLoader.hpp # loader interface for flat binaries
├── progs/                   # RV32 test programs + generated binaries
│   ├── link_rv32.ld         # linker script (places _start at 0x0)
│   ├── core/                # core bring-up / ISA tests (smurf, smexit, etc.)
│   ├── sci/                 # small scientific kernels
│   ├── *.elf / *.bin        # generated outputs (e.g., smurf.bin, hmm_step.bin)
│   └── .smile_dbg           # optional debugger breakpoint file
├── docs/
│   └── accel_port.md        # shared v1 accelerator contract
└── CMakeLists.txt           # build rules for tile1/tb_tile1 + smile_progs
```

## Current core test programs (`smile/progs/core/`)

These are the small, freestanding RV32 programs that are currently used with `smile` for micro-architectural and debugger testing.

- **`smexit.c`** – minimal exit sanity check  
  Issues an `ecall` with syscall ID 93 (put in a7 so system knows how to interpret this `ecall`) and an exit code (e.g., 7 in a0) almost immediately. Used to verify that:
  - ECALL handling works
  - `Tile1::request_exit()` is wired correctly
  - the simulator halts cleanly with the expected exit code

- **`smurf.c`** – main bring-up + trap-handling demo  
  Sets up a stack, installs a trap handler in `mtvec`, computes `SUM(0…9) = 45` into memory at `0x0100`, and sets a flag at `0x0104` depending on whether the sum is correct. It then:
  - triggers `ebreak` and `ecall`
  - uses `mcause` to distinguish breakpoint vs ecall
  - writes `0xBEEF` to `0x0108` on breakpoint and `0xDEAD` to `0x0104` on ecall

  This program exercises:
  - LW/SW and simple ALU arithmetic in a loop
  - conditional branches
  - CSR setup via `mtvec`
  - trap entry/exit (mepc/mcause, mret)

- **`smurf_debug.c`** – debugger REPL exercise  
  Writes known patterns to “scratch” locations at `0x0100` and `0x0104`, loads specific constants into registers (`t0`, `t1`, `s0`, `a0`), then executes `ebreak` followed by `ecall`. It is designed so that you can:
  - break into the debugger
  - inspect registers and memory from the REPL
  - confirm that exit carries the expected code

- **`smurf_threads.c`** – toy shared-sum “two-thread” demo  
  Two C functions (`thread0` and `thread1`) both update a shared global `sum`:
  - `thread0` adds 1 to `sum` five times, with an `ebreak` after each add
  - `thread1` adds 2 to `sum` five times, also breaking after each add  
  After both complete, the program ECALLs and returns `sum` as the exit code. This is useful for:
  - observing repeated traps/breakpoints in a loop
  - watching how shared memory updates show up in the debugger

All of these programs are linked with `progs/link_rv32.ld`, which places `_start` at address `0x00000000` so Tile1 can begin execution by simply starting at PC=0 after reset.

## Current scientific programs (`smile/progs/sci/`)

These are small scientific kernels.

- **`sum_lpv.c`** – LPV-style reduction
  Initializes an array of `N` 32-bit values at `0x0200` (`1,2,…,N`), computes their sum, stores the result at `0x0100`, and exits via ECALL 93 with the sum as the exit code.  Intended as a first “LPV-like” scalar kernel to study instruction mix and memory access patterns on `Tile1`.

## Core Pieces
- `Tile1` (`Tile1.hpp/cpp`): the RV32 core implementation
  - Files: `include/Tile1.hpp`, `src/Tile1.cpp`, `src/Tile1_exec.cpp`, `include/Instruction.hpp`, `src/Instruction.cpp`
  - Role: implements a simple RV32I core
    - one main entrypoint per cycle: `void Tile1::tick()` 
    - standard logical handling sequence: fetch, decode, execute, trap/PC update
  - Notes:
    - uses `MemoryPort` to talk to memory (abstract interface, `Tile1` never talks directly to DRAM)
    - uses `Instruction` for decoding RV32I instructions
    - uses exec_* helpers in `Tile1_exec.cpp` for ALU, loads, branches, CSR, custom0
- `MemoryPort`: a general protocol link for `Tile1` to talk to memories through
  - Files: `include/Tile1.hpp` (abstract class defined here), `src/tb_tile1.cpp` (concrete implementation for a Dram model)
  - Role: abstract memory interface for allowing `Tile1` to access all sorts of memory backends  
    - exposes simple word-oriented API: `read32(addr)`, `write32(addr, value)`.
    - designed to be synchronous (0-delay) for simplicity.
- Accelerators: 
  - Files: `include/AccelPort.hpp`, `include/AccelArraySum.hpp/cpp`, `include/AccelDemoAdd.hpp/cpp`
  - Role: simple accelerators that `Tile1` can call via custom0 instructions
    - `AccelPort`: abstract interface for “something that can take a CUSTOM-0 instruction and optional memory access.”
    - `AccelArraySum`: an example accelerator that sums an array in memory.
- `Debugger`: 
  - Files: `include/Debugger.hpp`, `src/Debugger.cpp`
  - Role: a simple REPL debugger for stepping through instructions, setting breakpoints, and inspecting state
    - connected to `Tile1` to read registers, PC, and memory via `MemoryPort`.
- `Testbench`:
  - Files: `src/tb_tile1.cpp`
  - Role: a simple testbench that instantiates `Tile1`, `Dram`, and `Debugger` and runs the simulation. 
    - loads a flat binary program into memory
    - runs the simulation loop, calling `Tile1::tick()` and `Debugger::run()`

## `tb_tile1` quick reference

`tb_tile1` is the standalone runner for `smile`: it constructs `Tile1`, memory (`DramMemoryPort` + `MemCtrlTimedPort`), optional accelerator, and the debugger loop.  
If `-prog` is provided, it loads that flat binary and runs it. If `-prog` is empty, it injects a built-in suite program selected by `-suite`.

| Option | Default | Meaning |
|---|---|---|
| `-showcontexts` | `0` | Print Cascade component contexts and exit. |
| `-prog=<path>` | `""` | Path to flat `.bin` to load. When set, suite injection is skipped. |
| `-load_addr=<hex/int>` | `0x0` | Address where the program image is written. |
| `-start_pc=<hex/int>` | `0x0` | Initial PC override. If `0`, injected suites start at `load_addr`. |
| `-mem_latency=<n>` | `0` | Fixed latency (cycles) used by `MemCtrlTimedPort`. |
| `-ideal_mem=<0/1>` | `0` | Force Tile1 ideal memory mode (sync read/write, no request/response stalls). |
| `-mem_model=timed\|ideal` | `timed` | Tile1 memory model selection. |
| `-accel=none\|demo_add`<br>`\|array_sum`<br>`\|array_sum_mc` | `array_sum` | Accelerator attached to CUSTOM-0. |
| `-suite=proto_accel_sum`<br>`\|proto_accel_sum_altaddr`<br>`\|proto_accel_sum_badarg`<br>`\|proto_accel_sum_unsupported`<br>`\|proto_accel_sum_twice` | `proto_accel_sum` | Built-in injected test suite used only when `-prog` is empty. |
| `-selfcheck=<0/1>` | `0` | Run built-in regression matrix across accel/suite/memory latency; exits nonzero on failure. |
| `-steps=<n>` | `0` | Auto-run for `n` cycles; `<=0` enters interactive debugger REPL. |
| `-sw_threads=<1|2>` | `1` | Number of software thread contexts scheduled by the debugger. |
| `-ignore_bpfile=<0/1>` | `0` | Do not load `.smile_dbg` breakpoints on startup. |

## Example usage

### Build `tb_tile1`

```bash
cd smarc
smarc $ cmake -S . -B build -DCEDAR_DIR=/path/to/Cascade/cedar -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
smarc $ cmake --build build --target tb_tile1 -j
```

### Run Built-In Suites (No `-prog`)

Run the default injected suite (`proto_accel_sum`):
```bash
./build/smile/tb_tile1 -steps=50 -sw_threads=1
```

Run an explicit injected suite:
```bash
./build/smile/tb_tile1 -suite=proto_accel_sum_twice -accel=array_sum_mc -mem_latency=5 -steps=800
```

Recent validated proto runs:
```bash
./build/smile/tb_tile1 -accel=array_sum -suite=proto_accel_sum -steps=200
./build/smile/tb_tile1 -accel=array_sum_mc -mem_latency=5 -suite=proto_accel_sum -steps=400
./build/smile/tb_tile1 -accel=array_sum_mc -mem_latency=5 -suite=proto_accel_sum_twice -steps=800
./build/smile/tb_tile1 -accel=array_sum_mc -mem_latency=5 -suite=proto_accel_sum_badarg -steps=200
./build/smile/tb_tile1 -accel=array_sum_mc -mem_latency=5 -suite=proto_accel_sum_unsupported -steps=200
```

### Selfcheck Regression (`-selfcheck`)

Run the built-in regression matrix (non-interactive). It returns exit code `0` when all cases pass, otherwise `1`:
```bash
./build/smile/tb_tile1 -selfcheck=1
```

### Run External Program (`-prog`)

Build a flat binary and run it:
```bash
smarc $ cd smile/progs
smarc/smile/progs $ riscv64-unknown-elf-gcc -march=rv32i_zicsr -mabi=ilp32 -nostartfiles -nostdlib -T link_rv32.ld core/smurf.c -o prog.elf
smarc/smile/progs $ riscv64-unknown-elf-objcopy -O binary prog.elf prog.bin
smarc/smile/progs $ cd ../..
smarc $ ./build/smile/tb_tile1 -prog=smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0 -steps=100 -sw_threads=1
```

Build and run the accelerator smoke test (`accel_sum_test`) via `smile_progs`:
```bash
# You don't have to build .bin one-by-one; the smile_progs target builds them all.
smarc $ cmake --build build --target smile_progs -j
smarc $ ./build/smile/tb_tile1 -prog=smile/progs/accel_sum_test.bin -accel=array_sum_mc -mem_latency=5 -steps=5000
smarc $ ./build/smile/tb_tile1 -prog=smile/progs/accel_sum_test.bin -accel=array_sum -steps=2000
```

Expected for both runs:
```text
[EXIT] Program exited with code 136
```

### Interactive Debugger REPL

Launch the debugger (no `-steps`):
```bash
./build/smile/tb_tile1 -prog=smile/progs/prog.bin -load_addr=0x0 -start_pc=0x0 -sw_threads=1
# then in the REPL:
smile> step
smile> regs
smile> mem 0x100 4
smile> break 0x10
smile> cont
smile> exit
```

### Multithread Scheduling (`-sw_threads`)

`tb_tile1` can schedule one or two software thread contexts in the debugger loop:

- `-sw_threads=1` (default): single-context execution (recommended for performance and instruction counting).
- `-sw_threads=2`: round-robin two contexts; useful for multithread/debug experiments.

Example:

```bash
# Single-thread (baseline perf counters)
./build/smile/tb_tile1 -prog=smile/progs/prog.bin -steps=2000000 -sw_threads=1

# Two software contexts (typically doubles instruction/counter totals if both run same program)
./build/smile/tb_tile1 -prog=smile/progs/prog.bin -steps=2000000 -sw_threads=2
```

## Relationship to smicro
- smile focuses on
  - a single core (`Tile1`)
  - instruction set
  - accelerators
  - debugger REPL
- smicro focuses on
  - SoC wiring (MemCtrl, DRAM, suites),
  - memory protocols (MemReq/MemResp),
  - different topologies and test suites

The same `Tile1` + `Instruction` + `Tile1_exec` + accelerator code is meant to be shared between smile and smicro. The only thing that changes is how the MemoryPort is implemented and how the core gets clocked.
