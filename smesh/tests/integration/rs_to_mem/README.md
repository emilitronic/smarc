# RS-to-memory integration tests

This directory holds integration tests for the domain between the
reservation station's command input and the internal-memory boundary
(scratchpad/accumulator) — i.e. everything `LdCtrl`/`ExCtrl`/`StCtrl` sit
between. The name `rs_to_mem` is meant to generalize: this is one slice of
that domain (the `ExCtrl` slice), with room for `ld_ctrl`/`st_ctrl`-focused
suites later without renaming anything.

## Why this is a different kind of integration test than `ex_ctrl`'s

`smesh`'s tests fall into three tiers, and it's worth being explicit about
which is which since two of them both live under `integration/`:

- **`unit/`** — one standalone block. Neighbors are emulated or absent
  (or, for a block like `SmeshRS` that is mostly plain C++ logic rather
  than a graph of Cascade sub-components, no simulation is run at all —
  see `unit/rs/tb_smesh_rs.cpp`).
- **`integration/ex_ctrl/`** — `ExCtrl`'s real internals (it is itself a
  compound sub-system of many Cascade components) wired together as a
  whole, with the *outside world* emulated (a synthetic RS-like command
  injector, a fixed-latency scratchpad stand-in). This is warranted because
  `ExCtrl` alone has enough internal complexity and latency-sensitivity to
  be a system in its own right.
- **`integration/rs_to_mem/`** (here) — the inverse. `ExCtrl` is treated as
  an already-validated, opaque block — nothing here re-checks its
  internals, it trusts what `ex_ctrl`'s suite already proved. What's real
  instead is everything *around* it: `SmeshCmdQueue`, `SmeshUnrolledCmdQueue`,
  the real `SmeshRS`, and the real `Spad`/`Accum` and their arbiters.

Concretely, this category of test exists to catch cross-component timing,
backpressure, and arbitration issues that neither of the other two tiers
can see — problems that only appear when real components actually contend
for a shared resource at the same time. Two known, real examples from this
project motivated writing these tests at all:

- `ArbWriteSpad`/`ArbWriteAccum` advertise "ready" on every write source
  unconditionally, even though only one is actually serviced per cycle —
  a concurrent second producer is told its write was accepted and then
  silently dropped. A unit test of the arbiter alone (single producer at a
  time) would never see this.
- `Spad`/`Accum` themselves can only service one bank's read (or write)
  per cycle, globally, despite exposing a fully banked interface — the
  same class of problem, one layer down.

Neither of these is reachable from `ex_ctrl`'s own suite, since it never
instantiates the real `Spad`/`Accum` or arbiters at all.

## Scope of what's tested here

Composition: `cmd_valid`/`cmd_bits` → `SmeshCmdQueue` → `SmeshUnrolledCmdQueue`
→ `SmeshRS` → `ExCtrl` → (real `Spad`/`Accum` read/write arbiters) →
`Spad`/`Accum`. `SmeshRS`'s load/store issue ports are enabled but never
used (the test programs are entirely `CONFIG_EX`/`PRELOAD`/`COMPUTE_*`, all
classified `Execute`), and are tied off. No `LdCtrl`/`StCtrl`/DMA/Mvin path
is present.

One small piece of glue exists only in this test target, not in `smesh/src`:

- A write-side legacy-struct adapter (`SpadBankWriteReq`/`AccumBankWriteReq`
  → `DmaReadResp`) — this has **no existing counterpart anywhere**, because
  `SmeshTop.cpp` currently ties `ExCtrl`'s write ports to a permanent-zero
  stub rather than wiring them to the arbiters at all. This test is the
  first place `ExCtrl`'s writeback path has ever been connected end-to-end
  to real `Spad`/`Accum`.

Programs tested: `basic` and `mul_pre`, the same two scenarios `ex_ctrl`'s
suite already validates in isolation — here issued as real `SmeshCmd`s
through the real queue+RS path instead of hand-injected `SmeshIssue`s, and
checked by (a) every expected RS-assigned completion tag appearing exactly once (order-independent), observed on
`ExCtrl.completed_val`/`completed_bits`, and (b) the final matmul result
values read back from the real `Accum`.
