smicro
=====

A scaffold for a RISC-V SoC + NN accelerator in Cascade.  We're just methodically building up an SoC for bio-AI and testing/evaluating it as we go along.

- SoC top maintains a simple cycle counter and accepts a `--topo` param:
  - `via_l1`, `via_l2` (default), `dram`, `priv`.
- Interactive TB: press Enter to step, `0` to reset, `q` to quit.

## Configure & Build
```bash
# Configure from repo root (i.e., from cedar)
cmake -S . -B build
# Build smicro
cmake --build build --target smicro -j
# from VS Code
Command + Shift + B
```

## Test Structure
A layered test structure is used to decouple test failure causes and keep tests fast.  There are two main types of tests * Hardware Abstraction Layer (HAL) tests and 
* Protocol tests.<br>
HAL tests: direct component pokes (e.g., DRAM). No clocking. No MemCtrl. Content/bounds only.<br>
Protocol tests: traffic over the real path: Source → MemCtrl → DRAM, with latency, posted-acks, RAW, etc.

The tests are organized across several layers of varying complexity.
### Layer 0 — Smoke (a protocol test)
Purpose: prove the sim ticks and wiring is sane.<br>
Scope: Core does one fixed store→load readback.<br>
Value: runs every build in seconds; catches broken clock, ports, ctor macros, trace filters.
### Layer 1 — HAL content/bounds, no timing
Purpose: prove DRAM bytes and address map is ok, independent of timing.<br>
Scope: Dram::write/read only; runs at t=0 (no MemCtrl involvement) via direct host calls.<br>
Value: if this fails, the bug is in DRAM or the map, not the controller or timing.<br>
### Layer 2 — Protocol/timing (MemCtrl→DRAM)
Purpose: prove latency, ordering, posted-ack policy, RAW forwarding.<br>
Scope: MemTester drives real requests through MemCtrl.<br>
Value: if this fails, the bug is in MemCtrl/plumbing, not storage, use this to validate timing/order guarantees or MemCtrl behaviour.
### Where it’s going
* Broaden L2: posted_on/off suites, WAW last-wins + fence, backpressure, burst sizes, byte-enable forwarding.
* Layer 3 (later): multiple masters (add accelerator), tags/IDs, arbitration fairness, OoO completion.
* Cache reintegration: reinsert L2 then L1; keep L1/L2 tests separate from DRAM HAL tests.
* CI matrix: run L0+L1 on every commit; run L2/L3 nightly with latency sweeps and randomized scripts.
* Single entrypoint: optionally unify flags to -suite=hal_* | proto_* to reduce confusion.
### Invocation
Single switch `-suite` selects what runs. HAL suites operate at t=0 via DRAM HAL only (no driver traffic). Protocol suites exercise timing over cycles.

- HAL: `hal_none | hal_multi | hal_bounds`
- Protocol (core): `proto_core` (RvCore issues its smoke sequence)
- Protocol (tester): `proto_raw | proto_no_raw | proto_rar | proto_lat`

## Mode Matrix (Cheat Sheet)

Quick view of `-suite` and what runs. Contexts refer to component instance names for `-trace`.

- HAL: `hal_none|hal_multi|hal_bounds` → DRAM HAL at t=0 only (no protocol traffic).
- Protocol (core): `proto_core` → RvCore smoke sequence (store→load readback).
- Protocol (tester): `proto_raw|proto_no_raw|proto_rar|proto_lat` → MemTester drives MemCtrl over cycles.

Recommended traces:
- Minimal: `-trace "SoC"` (ticks only).
- Core+MemCtrl: `-trace "SoC.RvCore;SoC.mem"`.
- Full path: `-trace "SoC;SoC.RvCore;SoC.mem;SoC.Dram"`.

## Quickstart Recipes

- Watch latency N on no-RAW path:
  - `./smicro -suite=proto_no_raw -mem_latency=2 -trace "SoC;SoC.mem" -steps=12`
- RAW forward (store→load same addr same tick):
  - `./smicro -suite=proto_raw -trace "SoC;SoC.mem;SoC.RvCore" -steps=10`
- HAL multi-address content check (t=0):
  - `./smicro -suite=hal_multi -trace "SoC.Dram" -steps=1`
- Interactive smoke:
  - `./smicro -suite=proto_core -trace "SoC;SoC.RvCore"`

## Flags → Code Path

- `-suite` is parsed in `smicro/src/tb_smicro.cpp` and normalized (legacy flags map to new suites).
  - HAL tests: `run_hal(S, soc)` executes at t=0 via `Dram::read/write` (no MemCtrl timing).
  - Protocol suites: `run_suite(S, soc, mem_lat)` issue MemTester ops over cycles (except `proto_core`, which uses RvCore).
- Components and wiring:
  - SoC top: `smicro/src/SoC.hpp` + `smicro/src/SoC.cpp`.
  - Core smoke FSM: `smicro/src/RvCore.cpp`.
  - MemCtrl timing/ACK/RAW: `smicro/src/MemCtrl.cpp`.
  - DRAM storage+HAL: `smicro/src/Dram.cpp`.

## Timing Clarification

- MemCtrl owns latency (`-mem_latency`), DRAM is zero-latency, FIFOs are 0/0 delay.
- With the current update ordering (age → issue → accept within one tick), loads see one staging cycle when `mem_latency` would otherwise be 0.
  - For `mem_latency = 0`: `t_got − t_sent ≈ 1`.
  - For `mem_latency ≥ 1`: `t_got − t_sent ≈ mem_latency`.
- Compact rule: `delta = max(1, mem_latency)` under the default 0/0 FIFOs and zero-latency DRAM.
- The testbench assertions still accept both `delta == L` or `delta == L+1` in `proto_no_raw` and `proto_lat` to tolerate future scheduling flips.

## Run smicro 
Note that Cascade's tracing system needs the trace key to be enabled (with `-trace`) to emit output.  Trace filters match component instance names (contexts), not classes.  Use `-showcontexts` to list contexts, or use known instance names in filters.  Note: `-showtraces`/`-showkeys` list named trace keys, not component contexts. Since this demo uses anonymous `trace("...")` calls (no named keys), `-showtraces` typically only prints built‑in keys like `checkpoint`.

```bash
# Interactive mode; press return to step, q to quit
./build/smicro/smicro -suite=proto_core -trace "SoC"
# Run 3 steps then exit (from build/smicro dir)
./smicro -suite=proto_core -trace "SoC" -steps=3
# Typical contexts (verify with -showcontexts): SoC, SoC.RvCore, SoC.mem, SoC.Dram
./smicro -suite=proto_core -trace "SoC;*.RvCore;*.Dram" -steps=25
# Specify topology and show all traces
./smicro -suite=proto_core -topo=dram -trace "*" -steps=25
# Set MemCtrl latency
./smicro -suite=proto_core -trace "SoC.RvCore;SoC" -steps=20 -mem_latency=1
# See MemCtrl/DRAM timing (tester-driven)
./smicro -suite=proto_no_raw -trace "SoC.RvCore;SoC.Dram;SoC.mem;SoC" -steps=10 -mem_latency=1
# Show component instances
./smicro -showcontexts
# Posted writes on, proto_core
./smicro -suite=proto_core -trace "SoC.RvCore;SoC.Dram;SoC" -steps=11 -mem_latency=2 -posted_writes=1
# HAL multi-address content check (t=0 only)
./smicro -suite=hal_multi -trace "SoC.Dram" -steps=1
# HAL bounds check (t=0 only)
./smicro -suite=hal_bounds -trace "SoC.Dram" -steps=1
# Protocol RAW forward (same-tick store→load A)
./smicro -suite=proto_raw -trace "SoC;SoC.RvCore;SoC.mem;SoC.Dram" -steps=11
# Protocol no-RAW (store A, load B; expect ≈ max(1, mem_latency) cycles)
./smicro -suite=proto_no_raw -trace "SoC;SoC.RvCore;SoC.mem;SoC.Dram" -steps=11 -mem_latency=3
# Protocol RAR idempotence (two loads of A match)
./smicro -suite=proto_rar -trace "SoC;SoC.RvCore;SoC.mem;SoC.Dram" -steps=11
# Protocol latency sweep (L in {0,1,3,7}, checks ≈ L(+1))
./smicro -suite=proto_lat -trace "SoC;SoC.RvCore;SoC.mem;SoC.Dram" -steps=40
```

## Build & Run Separate HAL (broken)
```bash
cmake --build build --target test_vectadd -j && ./build/smicro/test_vectadd
```

## Parameters

These are parsed by descore::Parameter. Note: non-boolean parameters require an equals sign (`-name=value`).

- `-suite=<hal_none|hal_multi|hal_bounds|proto_core|proto_raw|proto_no_raw|proto_rar|proto_lat>`: Selects HAL vs protocol suite.
- `-topo=<via_l1|via_l2|dram|priv>`: Selects core/accel topology (default `via_l2`).
- `-steps=<N>`: Batch N cycles then exit; omit for interactive stepping.
- `-trace="<component filters>"`: Component context filters (e.g., `SoC.RvCore;SoC.Dram` or `*.RvCore;*.Dram`).
- `-mem_latency=<N>`: MemCtrl latency in cycles (default 3). Example: `./build/smicro/smicro -suite=proto_no_raw -trace "SoC.RvCore;SoC.mem" -steps=40 -mem_latency=5`
- `-dram_latency=<N>`: Deprecated alias; if provided (>=0) overrides `-mem_latency`.
- `-posted_writes=<0|1>`: Enable posted write ACKs (default 1). If 0, stores ACK when they drain to DRAM.
- `-drain`: After batch run, keep stepping until posted stores drain (fence).
- `-showcontexts`: List component instance names and exit.

## Latency & Delays (default: split‑update + MemCtrl)

- MemCtrl latency: `-mem_latency=<N>` sets timing within MemCtrl (applies to next accepted request).
- FIFO delays (SoC wiring): both DRAM-facing FIFOs use 0 delay (`s_req=0`, `s_resp=0`).
- Split‑update core: separate `UPDATE(update_req)` and `UPDATE(update_resp)` let the scheduler order req→mem→dram→mem→resp with no comb loop.
- Effective load timing: because `MemCtrl::update_issue()` ages the queue before taking a new request, any request enqueued with `cnt=0` waits one extra tick before issuing. With DRAM at zero latency and 0/0 FIFO delays this yields `t_got − t_sent ≈ max(1, mem_latency)`.
- Examples (with `-trace "SoC.RvCore;SoC.mem"`):
  - `-mem_latency=0` → delta ≈ 1 cycle
  - `-mem_latency=1` → delta ≈ 1 cycle
  - `-mem_latency=10` → delta ≈ 10 cycles
- DRAM debug traces (enabled by `SoC.Dram`):
  - `dram: accept @<cnt> addr=0x...` when a request is popped and countdown (`cnt=latency`) starts.
  - `dram: respond addr=0x...` when the response is pushed.

Historical notes:
- Earlier single‑update fallback: with a 1‑cycle response FIFO (`s_req=0`, `s_resp=1`), `t_got − t_sent ≈ latency + 2`.
- Earlier DRAM‑owned latency: when DRAM modeled staging internally, `t_got − t_sent ≈ dram_latency + 1` even with 0/0 FIFOs.

## MemCtrl

MemCtrl owns timing, arbitration, and write‑acks; DRAM is pure storage.

- Responsibilities
  - Ingress FIFOs per master (currently core only).
  - Latency pipeline (`-mem_latency=N`) before issuing to DRAM.
  - Write ACK policy: posted by default (`-posted_writes=1`). If disabled, ACK when store drains to DRAM.
  - Response routing via `id` tag (single master path currently uses `id=0`).
- Wiring (SoC)
  - `RvCore.m_req -> MemCtrl.in_core_req`
  - `MemCtrl.out_core_resp -> RvCore.m_resp`
  - `MemCtrl.s_req -> DRAM.s_req`
  - `DRAM.s_resp -> MemCtrl.s_resp`
- DRAM defaults to zero latency and acts as a byte array; row/bank timing can be added later without touching core.

### Diagram

```
         RvCore                          MemCtrl                         DRAM (zero‑latency)
   +----------------+            +---------------------+                 +----------------+
   |                |  m_req     | in_core_req   s_req |  s_req          |                |
   |        m_req ==+===========>+==> [ latency N ] ===+=================>+==> s_req      |
   |                |            |                     |                 |   (apply store |
   |        m_resp  |  m_resp    | out_core_resp s_resp|  s_resp         |    or read)    |
   |  <============ +<===========+=====================+<================+<== s_resp      |
   +----------------+            +---------------------+                 +----------------+
```

Notes
- With core split updates and 0/0 FIFO delays, the scheduler orders req→mem→dram→mem→resp in one tick where possible.
- Because `update_issue()` issues before accepting a fresh request, any enqueue happens after the issue window for that tick; with split updates that translates to `t_got − t_sent ≈ max(1, mem_latency)`.

## Memory Map

- DRAM: 0x8000_0000 – 0x8FFF_FFFF (256 MiB mapped by default in the demo).
- L1/L2: logical path only, not memory‑mapped address ranges.

---

## Architecture

The SoC consists of the following components:

*   **1 RISC-V Core:** The main processor of the system.
*   **L1 Cache:** A level-1 cache for the RISC-V core.
*   **L2 Cache:** A level-2 cache.
*   **DRAM:** The main memory of the system.
*   **Neural Network Accelerator:** A hardware accelerator for neural network operations, specifically matrix multiplication for CNNs and ReLU functions.

## Accelerator Connectivity

The Neural Network Accelerator has flexible memory connection options, allowing for different performance and complexity trade-offs. It can be connected:

*   Through the L1 cache
*   Through the L2 cache
*   Directly to DRAM
*   To its own dedicated DRAM
