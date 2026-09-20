# RS-to-memory integration tests

This suite sends raw `SmeshCmd` instructions through the real top-level
`Smesh` composition. It covers the path from command acceptance through the
RS and ExCtrl to the real Spad/Accum blocks, including the production
arbitration and memory wiring.

## Scope

The `ex_ctrl` integration suite focuses on ExCtrl itself, with its neighbors
modeled by the harness. This suite instead instantiates `Smesh`, so it checks
the composition and handshakes between the real blocks around ExCtrl.
That makes it complementary to the unit tests and the ExCtrl suite: it catches
integration issues such as bank arbitration and backpressure that an isolated
ExCtrl simulation cannot expose.

The current scenarios issue `CONFIG_EX`, `PRELOAD`, and `COMPUTE_*` commands.
They read operands from Spad and write results to Accum, but do not issue load
or store commands. The test harness therefore initializes the behavioral
Spad image directly before the first simulated cycle. The external memory
boundary is connected to MemCtrl/Dram because Smesh requires it, but that
path remains idle in these cases. Accum results and RS completion tags are
checked after each run.

The suite currently covers:

- `basic`: PRELOAD followed by COMPUTE_FLIP and COMPUTE_STAY.
- `mul_pre`: two operations using the overlapping COMPUTE+PRELOAD path.
- `concurrent_banks`: checks that Spad reads can fire on three banks at once.
- `same_bank_serializes`: checks that aliased A/addend reads serialize.

The focused memory unit tests cover bank-local arbitration and simultaneous
operations independently. This suite also checks Spad read concurrency and
same-bank serialization through the full command-to-memory composition.

## Run

From the repository root:

```bash
cmake --build build --target tb_rs_to_mem_suite -j
./build/smesh/tb_rs_to_mem_suite -list_tests
./build/smesh/tb_rs_to_mem_suite -test=basic
./build/smesh/tb_rs_to_mem_suite -test=mul_pre
ctest --test-dir build -L rs_to_mem --output-on-failure
```

Use `-trace` with the suite executable to inspect production component traces.
The harness also checks expected completion tags, final Accum values, and
configured Spad bank-concurrency bounds.
