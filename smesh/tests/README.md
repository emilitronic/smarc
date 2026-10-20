# Smesh tests

This directory contains tests for the Smesh hardware model. Some tests check
one component at a time; others send commands through connected parts of the
system and check the resulting data and completions.

## Test groups

- `unit/`: focused tests for individual components, including ExCtrl (execute controller), LdCtrl (load
  controller), StCtrl (store controller), local memories, DMA, the mesh, and the loop generators.
- `integration/`: tests that connect components into larger paths. For example,
  `integration/rs_to_mem` sends commands through the RS and memory paths, while
  `integration/ex_ctrl` focuses on ExCtrl with neighboring components modeled
  by its test harness.
- `reference/`: software reference programs used to inspect generated command
  sequences and expected behaviour.
- `legacy/`: older Smesh testbenches retained for reference or existing use.
- `support/`: testbench helpers, including performance measurement code.

## Running tests

CTest is the catalog and launcher for tests registered by CMake. From the
repository root, use:

```bash
cmake --build build -j
ctest --test-dir build -N
ctest --test-dir build --output-on-failure
ctest --test-dir build -L unit --output-on-failure
ctest --test-dir build -L integration --output-on-failure
```

To run one registered test, match its full CTest name:

```bash
ctest --test-dir build -R '^smesh_unit_spad_banked$' --output-on-failure
ctest --test-dir build -R '^smesh_rs_to_mem_loop_conv_temporal3$' --output-on-failure
```

To run a small selection, use a regular expression:

```bash
ctest --test-dir build -R '^smesh_rs_to_mem_(basic|loop_conv_temporal3)$' --output-on-failure
```

To inspect a test with traces or choose a scenario interactively, run its suite
executable directly. For example:

```bash
./build/smesh/tb_rs_to_mem_suite -test=loop_conv_temporal3 -trace '*/rs_mem_conv_execute_'
```

Here `loop_conv_temporal3` is the scenario name accepted by the executable;
CTest registers that same case as `smesh_rs_to_mem_loop_conv_temporal3`.

The integration-suite READMEs describe their scenarios and trace commands.
