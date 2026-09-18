# ExCtrl Integration Tests

The maintained ExCtrl integration runner is `tb_ex_ctrl_suite`. It instantiates
the complete `ExCtrl` together with an RS-like command driver, a fixed-latency
scratchpad model, and result checkers.

## Build and Run

Run these commands from the `smarc` repository root:

```bash
cmake --build build --target tb_ex_ctrl_suite -j

./build/smesh/tb_ex_ctrl_suite -list_tests
./build/smesh/tb_ex_ctrl_suite -test=basic
./build/smesh/tb_ex_ctrl_suite -test=mul_pre
./build/smesh/tb_ex_ctrl_suite -test=mul_pre -trace '*'/ex_ctrl_suite_
```

Useful reusable-harness traces are:

```bash
# FSM state and the three visible command-queue entries.
./build/smesh/tb_ex_ctrl_suite -test=basic -trace '*/ex_ctrl_view'

# Scratchpad requests, offered responses, and accepted responses.
./build/smesh/tb_ex_ctrl_suite -test=basic -trace '*/ex_ctrl_spad_mem_view'

# Scratchpad and accumulator writes.
./build/smesh/tb_ex_ctrl_suite -test=basic -trace '*/ex_ctrl_write_mem_view'

# Completion tags in the order reported to the RS.
./build/smesh/tb_ex_ctrl_suite -test=basic -trace '*/ex_ctrl_completed_view'
```

Production-component traces such as `row_feed_`, `mq_`, `mesher_req_`,
`mesher_in_`, and `mesher_resp_` can be selected from this runner as well.

CTest launches each scenario in a fresh process:

```bash
# Run one scenario.
ctest --test-dir build -R '^smesh_ex_ctrl_basic$' --output-on-failure

# Run both maintained ExCtrl integration scenarios.
ctest --test-dir build -R '^smesh_ex_ctrl_(basic|mul_pre)$' --output-on-failure

# Run all integration tests, including future non-ExCtrl integration tests.
ctest --test-dir build -L integration --output-on-failure

# Run every ExCtrl test, both unit and integration.
ctest --test-dir build -L ex_ctrl -j 2 --output-on-failure
```

## Maintained Scenarios

### `basic`

Program:

```text
CONFIG -> PRELOAD -> COMPUTE_FLIP -> COMPUTE_STAY
```

Checks scratchpad requests and responses, Mesher progress, accumulator
writeback, and the exact completion order of all four command tags. The initial PRELOAD
provides the destination for the first compute result. The trailing standalone
COMPUTE_STAY has no following PRELOAD, so its result is not written.

### `mul_pre`

Program:

```text
CONFIG -> PRELOAD0 -> COMPUTE_FLIP0 + PRELOAD1 -> COMPUTE_FLIP1
```

Checks the overlapping COMPUTE/PRELOAD path, including both weight sets, both
result matrices, scratchpad traffic, Mesher progress, accumulator writeback,
and the exact completion order of all five command tags.

Scenario definitions, memory contents, and expected results are in
`tb_ex_ctrl_test_cases.cpp`. The reusable test environment is in
`tb_ex_ctrl_harness.cpp` and `tb_ex_ctrl_harness.hpp`.

## Older Tests in `smesh/src`

These tests predate the reusable suite and are not registered with CTest.

| Test | Relationship to the suite | Current status |
|---|---|---|
| `tb_ex_ctrl.cpp` | Predecessor of `basic`; its useful traces and end-to-end checks have been migrated into the reusable harness. | Superseded; retained only until explicitly removed. |
| `tb_ex_ctrl_mul_pre.cpp` | Predecessor of `mul_pre`; contains its own command driver, SPAD model, and cumulative progress trace. | Passes, but substantially overlaps `mul_pre`. |
| `tb_ex_ctrl_arch.cpp` | Small RS-like command/completion test using CONFIG, helper PRELOAD, CONFIG. | Currently fails and should not be treated as an active regression. |

`smesh/src/tb_ex_ctrl_scenarios.hpp` supplies the scenario used by the older
`tb_ex_ctrl.cpp`; it is not a separate executable test.

The reusable suite now provides the established `tb_ex_ctrl.cpp` diagnostic
traces and checks exact Mesher requests, A/B/D row-beats, response rows,
writeback, and completion order. New scenarios and checks should be added to
the maintained suite instead of the duplicated standalone harnesses.
