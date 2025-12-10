---
layout: default
title: smicro
---

# smicro

## Components and Roles (smicro + smile)

- `Tile1` (in `smile/`):
  - The actual RV32 CPU model.
  - Knows:
    - a clock (`clk`).
    - a `MemoryPort*` and optional accelerator port.
  - Does **not** know about:
    - `Dram`, `MemCtrl`, FIFOs, `SoC`, or testbenches.

- `Tile1Core` (in `smicro/`):
  - A thin wrapper to host `Tile1` inside smicro.
  - Responsibilities:
    - owns a `Tile1 tile_;`
    - for now, creates a `DramMemoryPort` adapter and calls `tile_.attach_memory(...)`.
    - implements `update()` (one `tile_.tick()` per cycle).
    - optionally implements `reset()` later.

- `Dram`:
  - Simulated DRAM + HAL helpers.
  - Two personalities:
    - **Protocol side**: `s_req/s_resp` FIFOs (MemCtrl talks to these).
    - **HAL side**: raw `read(addr, void*, bytes)` / `write(...)` for tests and adapters.

- `MemCtrl`:
  - Speaks `MemReq/MemResp` on the core side and `MemReq/MemResp` on the DRAM side.
  - Only in the path for `proto_*` suites that use the tester or (later) core via `m_req/m_resp`.

- `RvCore`:
  - A tiny FSM that **only** exists to exercise `MemCtrl` protocol (store then load).
  - Not a real CPU; it just pushes/consumes `MemReq/MemResp`.

- `SoC`:
  - The top-level composition: instantiates `Tile1Core`, `MemTester`, `MemCtrl`, `Dram`, etc.
  - Decides who is actually driving memory based on `use_test_driver` (derived from `-suite`).

## Memory Interfaces (3 Layers)

There are three different “views” of memory:

1. **CPU view – `MemoryPort`**  
   - API: `read32(addr)`, `write32(addr, value)`  
   - Used by: `Tile1`  
   - Goal: Simple, word-oriented, no FIFOs, no timing details.

2. **HAL / test view – `Dram::read/write`**  
   - API: `read(uint64_t addr, void* dst, uint64_t bytes)`, `write(...)`  
   - Used by: testbenches and adapters (`DramMemoryPort`, HAL tests in `tb_smicro`, `tb_tile1`)  
   - Goal: “Let me poke bytes into the DRAM array from the outside.”

3. **Protocol view – `MemReq/MemResp` FIFOs**  
   - API: `FifoOutput<MemReq>`, `FifoInput<MemResp>` with `push/pop/full/empty`  
   - Used by: `RvCore`, `MemTester`, `MemCtrl`, and (later) `Tile1Core` when it grows a proper LSU  
   - Goal: Exercise and model an on-chip memory protocol with latency/backpressure.

Current setup for `-suite=proto_core`:

- `Tile1` uses **(1)** via `MemoryPort`.
- `Tile1Core::DramMemoryPort` bridges **(1) → (2)**.
- The `MemReq/MemResp` (**3**) path is effectively idle (core’s `m_req/m_resp` are bit-bucketed).

### Call chain for an instruction fetch (proto_core, topo=dram)

Tile1::tick()
  → mem_port_->read32(pc_)
    → Tile1Core::DramMemoryPort::read32(pc_)
      → Dram::read(dram_base + pc_, &value, 4)

## Suites (`-suite`) and Drivers

We distinguish between:

- `hal_*` suites:
  - Only test DRAM via HAL at t=0 (no cycles, no driver).
  - `is_hal = true`, `use_tester = false`.

- `proto_*` suites:
  - Exercise protocol / timing over cycles.
  - `is_proto = true`.
  - `use_tester = (suite != "proto_core")`.

Mapping:

- `proto_core`:
  - `use_tester = false`.
  - SoC wiring:
    - `Tile1Core` is wired directly to `Dram` via `attach_dram()` and `DramMemoryPort`.
    - `MemTester` is disabled.
    - `MemCtrl` core-side ports are neutralized.
  - Effect: “Let the real CPU run against DRAM; MemCtrl is not in the path yet.”

- `proto_raw`, `proto_no_raw`, `proto_rar`, `proto_lat`:
  - `use_tester = true`.
  - SoC wiring:
    - `MemTester` ↔ `MemCtrl` ↔ `Dram`.
    - `Tile1Core` is present but its `m_req/m_resp` are sent to bit bucket / zero.
  - Effect: “Drive MemCtrl with scripted MemTester traffic and check timing/latency.”

  ## Unknown / Garbage Instructions

- Tile1 will interpret **any 32-bit word** as an instruction.
- The decode logic (`Instruction decoded(instr)`) always succeeds in producing a struct.
- The main `switch (decoded.category)` has a `default:` that just `break`s (no trap, no assert).
- Result:
  - Many garbage encodings effectively become NOPs or weird-but-legal ALU ops.
  - The core only “explodes” if:
    - an `assert_always` fires, or
    - some path calls `request_illegal_instruction()` and the trap handling leads to a halt.

This is why seeding DRAM with `0x1122334455667788` results in instructions like `0x55667788` and `0x11223344` that execute without immediate failure.