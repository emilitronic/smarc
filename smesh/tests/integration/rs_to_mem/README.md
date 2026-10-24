# RS-to-memory integration tests

This suite checks how commands move through the connected Smesh system and
whether they produce the expected results. The name `rs_to_mem` reflects its
original focus on commands moving from the reservation station (RS) through the
execution and memory paths. The suite now also covers LoopConv and LoopMatmul,
which accept higher-level commands before the RS.

## Scope

The tests contained in this suite exercise commands across the system: from command input and the reservation station, through the load, execute, and store paths, to scratchpad memory, accumulator memory, and externally  connected memory.  They check end-to-end results as well as interactions such as bank arbitration
and backpressure. Tests focused on ExCtrl (execute controller) alone are in
`smesh/tests/integration/ex_ctrl`.  In that case, the components surrounding ExCtrl
are modelled by the testbench itself.

The test cases in this suite fall into five groups:

- Direct ExCtrl command sequences using `CONFIG_EX`, `PRELOAD`, and `COMPUTE_*`.
- LoopMatmul workloads, with operands initialized in Spad or loaded from memory.
- LoopConv workloads with varied image and kernel shapes, channels, padding,
  and pixel repetition.
- Memory-path checks for zero fill, accumulator loads and scaling, and bank
  arbitration.
- Cross-controller interactions, such as ExCtrl and Store reading Accum at the
  same time.

The list below describes each test case and what it checks.

Some test cases provide primitive commands directly; others provide LoopMatmul
or LoopConv commands that are expanded into primitive commands before reaching
the RS.

## Testbench parts

- **Test runner** (`tb_rs_to_mem_suite.cpp`): the executable
  `tb_rs_to_mem_suite`. It selects the test case named by `-test`, initializes
  the simulation, runs clock cycles, and prints PASS or FAIL.
- **Test-case catalog** (`tb_rs_to_mem_test_cases.hpp/.cpp`): defines each
  test case as an `RsMemTestCase` record. The record holds its input commands,
  initial memory contents, expected results, and cycle limits. Names such as
  `basic` and `loop_conv_temporal3` select records from this catalog.
- **Harness** (`tb_rs_to_mem_harness.hpp/.cpp`): builds and connects the
  Smesh system, memory model, command driver, and result checks for one
  test case.
- **Command driver** (`RsMemCmdDriver`): sends the test case's commands into
  Smesh, holding each command until the input accepts it.
- **Monitor** (`tb_rs_to_mem_monitor.hpp/.cpp`): records completion tags,
  selected LoopConv commands, and Spad/Accum read activity. The runner creates
  `RsMemMonitor` alongside the harness, giving it access to Smesh. After each
  `Sim::run()`, the runner calls `monitor.sample()`. This calls the ordinary C++
  `CompletionMonitor`, `ConvExecMonitor`, and `LocalMemMonitor` classes to
  read the completed cycle's signals and record completions, commands, and
  local-memory activity. The runner calls `monitor.reset()` after `Sim::reset()`
  to clear all records for a new run.
  The monitor keeps command/tag lists,
  read totals, a peak Spad bank count, an Accum concurrency flag, and an ExCtrl
  Accum response stall count. It does not keep a per-cycle memory history.
  The harness's `passed(monitor)` and `report(monitor)` use those records
  alongside memory contents to check the test case's expectations.
- **Performance statistics** (`RsMemPerfStats` in `tb_rs_to_mem_perf.hpp/.cpp`): counts accepted
  input commands and calculates startup, execution, and drain cycles from
  observations supplied by the runner. It uses the shared counters in
  `smesh/tests/support/perf`.

### Monitor implementation choice

Commit `686f5b29d0e60cdfc30ee33f586604a98c1796ec`
(`Move local-memory sampling into a clocked monitor`) contains the version where
all three signal monitors are Cascade components, with input ports, declared
read dependencies, and clock connections. It provides a reference if we need
to revisit that approach.

We are now trying ordinary C++ monitors, sampled once after each `Sim::run()`.
The reason is simpler, more compact code: the monitors read Smesh directly,
without separate port declarations and wiring. Their measurement responsibilities
and the harness's result checks remain the same.

The timing tradeoff is that C++ sampling sees the signals available after
`Sim::run()` returns. It cannot observe an earlier value that was overwritten
during that call, or declare that a measurement must occur between particular
Cascade updates. A Cascade monitor can use its declared dependencies to place
its observation within the simulator's update order. For a future measurement
that requires such timing, check whether sampling after `Sim::run()` still
captures the intended event before choosing its implementation.

For the C++ conversion, all 34 relevant tests passed, and detailed reports from
ten representative cases matched the Cascade version exactly. This verifies
the observations exercised by those cases; it does not establish that every
future timing measurement can use the same sampling point. Command traces now
print when the runner samples, so their timestamps reflect that sampling point.

## Test cases

The suite currently covers the following cases:

**Direct ExCtrl commands and RS interactions**

- `basic`: PRELOAD followed by COMPUTE_FLIP and COMPUTE_STAY.
- `mul_pre`: two operations using the overlapping COMPUTE+PRELOAD path.
- `concurrent_banks`: checks that Spad reads can fire on three banks at once.
- `same_bank_serializes`: checks that aliased A/addend reads serialize.
- `transpose_a`: CONFIG_EX enables A transpose; PRELOAD and COMPUTE_FLIP
  produce `A^T * B + D` using signed, asymmetric matrices and a nonzero addend.
- `transpose_a_same_bank`: the same operation with input and weights in the
  same Spad bank, forcing their preload reads to serialize.
- `transpose_b`: ordinary PRELOAD/COMPUTE_FLIP commands produce `A * B^T + D`;
  ExCtrl's input unroller inserts the required helpers internally.
- `transpose_b_expanded`: the manually expanded diagnostic version, with
  ExCtrl's input unroller bypassed explicitly by the test configuration.

The A and expanded-B transpose cases also run in the DIM8 suite. They exercise
the real RS, ExCtrl configuration/operand reads, Spad response timing, Mesher's
shared transposer, and accumulator writeback. Expected results come from direct
matrix calculations; completion tags must all appear exactly once.

**B-transpose command expansion:** ExCtrl now instantiates
`TransposePreloadUnroller` ahead of its main command queue, as Original does.
The normal `transpose_b` test supplies only CONFIG_EX, PRELOAD B -> C, and
COMPUTE_FLIP A,D. After the RS issues the PRELOAD/COMPUTE pair, ExCtrl expands
it internally to:

```text
CONFIG_EX: B transpose enabled
PRELOAD B -> garbage destination       (fill the transposer)
COMPUTE_STAY garbage, garbage          (helper compute)
PRELOAD garbage -> C destination       (load transposed weights into the array)
COMPUTE_FLIP A, D                      (compute A * B^T + D)
```

The checked sequence requires the helper COMPUTE_STAY and following PRELOAD
to overlap inside ExCtrl; the integration test below checks that they do.
The inserted commands have invalid completion tags. Only the three original
RS commands complete; helpers do not consume RS entries. A transpose uses
CONFIG_EX/PRELOAD/COMPUTE_FLIP without needing this expansion.

For the older `transpose_b_expanded` diagnostic, all five commands above
enter the RS and receive tags. Its explicit `transpose_preload_unroll=false`
constructor setting avoids expanding an already-expanded sequence a second time.
Normal Smesh and ExCtrl instances enable the input unroller by default.

```bash
./build/smesh/tb_rs_to_mem_suite -test=transpose_a
./build/smesh/tb_rs_to_mem_suite -test=transpose_a_same_bank
./build/smesh/tb_rs_to_mem_suite -test=transpose_b
./build/smesh/tb_rs_to_mem_suite -test=transpose_b_expanded
./build/smesh/tb_rs_to_mem_suite_dim8 -test=transpose_a
./build/smesh/tb_rs_to_mem_suite_dim8 -test=transpose_b
ctest --test-dir build -R 'rs_to_mem.*transpose_' --output-on-failure
```

**Integrated unroller verification**

`tb_transpose_integration` checks the input unroller with one ordinary command
pair, a pair whose COMPUTE_FLIP arrives 40 cycles late, and sixteen consecutive
pairs that fill the main command queue. The pressure run must observe both
unroller-output and RS-input backpressure. All runs check every useful mesh row,
SPAD read counts, Accum write counts/addresses, delayed destination tags, and
exactly the original RS completions. Request counts also verify that the helper
does not execute separately from its following PRELOAD. These tests cover DIM=4
and DIM=8 with the current eight-entry ExCtrl command queue; other dimensions
or queue depths need their own verification.

```bash
cmake --build build --target tb_transpose_integration tb_transpose_integration_dim8 -j
./build/smesh/tb_transpose_integration
./build/smesh/tb_transpose_integration_dim8
```

**Transpose helper comparison (input unroller bypassed)**

`tb_transpose_helper` runs the expanded B-transpose program twice through Smesh:
once with the helper's actual operands retained, as in the inspected Original
source, and once with garbage operands, as our integrated unroller generates.
Original's helper assignments target `cmd.inst.rs1/rs2` (register-number fields),
leaving its `cmd.rs1/rs2` operand values unchanged. Smesh deliberately changes
the operand values instead. This test checks the effect in Smesh, not Original RTL.

Both runs must produce the independently calculated `A * B^T + D`, all five
RS completions exactly once, and exactly DIM Accum row writes. Three accepted
mesh requests establish that the helper and second PRELOAD overlapped. Request
destinations are garbage/C/garbage, but result destinations are garbage/garbage/C:
the helper's arithmetic is discarded through the delayed destination tag.

| Mesh size | SPAD row reads with real helper operands | With garbage operands |
| --- | ---: | ---: |
| DIM=4 | 20 | 12 |
| DIM=8 | 40 | 24 |

The reduction is DIM reads of A plus DIM reads of the addend D. Both versions
still read the weight matrix once. These are read counts, not cycle-speedup claims.

```bash
cmake --build build --target tb_transpose_helper tb_transpose_helper_dim8 -j
./build/smesh/tb_transpose_helper
./build/smesh/tb_transpose_helper_dim8
```

**Known scheduling limitation:** delaying the second PRELOAD until the helper
has completed forces four separate mesh requests. In Smesh this produces D alone,
with either helper variant: the extra standalone PRELOAD advances the transposer
again and replaces the weights before the final COMPUTE_FLIP. This is not caused
by suppressing the helper's operand reads. The integrated tests above verify that
normal unroller output preserves the overlap under the exercised input delays
and queue pressure. This forced-separated diagnostic bypasses that unroller.

The opt-in diagnostic below reproduces that numerical failure and returns a
nonzero exit status. It is not registered as a passing CTest regression:

```bash
./build/smesh/tb_transpose_helper -separate_helper
```

**LoopMatmul workloads**

- `loop_ws`: one tile with operands initialized in Spad.
- `loop_ws_dma`: one tile loaded from DRAM and stored back to DRAM.
- `loop_ws_dma_i2`: two output tiles with a shared B tile.
- `loop_ws_dma_k2`: two products accumulated into one output tile.

**LoopConv workloads**

- `loop_conv_dma`: one 1x4 NHWC image with four input channels, a 1x1 kernel
  producing two output channels, and signed full-width bias. Loads all operands
  from DRAM, checks the independently computed accumulator rows and stored
  output bytes, and requires both LoopConv slots and utilization counters to drain.
- `loop_conv_kernel2`: a 2x2 kernel over a 3x3 image with two input/output
  channels and bias. Two jobs each produce one row of the 2x2 output, using
  shared A rows 0-5 and the final eight B rows. Checks all
  four kernel contributions, RS protection of shared operands, accumulator
  results, stored output bytes, completions, and loop retirement.
- `loop_conv_temporal3`: a 1x6 image with two input channels, a 1x3 kernel,
  and two output channels. Valid stride-1 convolution produces four output
  positions. Checks all three temporal contributions, bias, loaded SPAD rows,
  accumulator results, DRAM bytes, completions, and loop/utilization drain.
- `loop_conv_temporal3_2x6`: the same 1x3 kernel and two input/output channels
  over a 2x6 image with different samples in each image row. Produces a 2x4
  output per channel. Checks input SPAD rows 0-11, weight rows 26-31 in the
  default 32-row SPAD, eight accumulator rows, DRAM bytes, completions, and
  loop/utilization drain. Horizontal windows must stay within their image row.
- `loop_conv_temporal3_padded`: the 1x6 image with one zero position at each
  end, producing six outputs per channel. Checks all eight input SPAD rows,
  the six weight rows, independent Accum results, DRAM bytes, completions, and
  loop retirement. Nonzero initial border rows and nonzero DRAM data at address
  zero ensure padding is generated locally rather than read from memory.
- `loop_conv_temporal3_pixel2` and `loop_conv_temporal3_padded_pixel2`: the
  same convolutions with `max_pixels_per_row=2`, using the separately compiled
  first-layer model. Check packed SPAD rows, the exact generated execution
  commands, independent convolution results in Accum and DRAM, and completions.
  The padded case poisons all four SPAD columns before loading, including the
  upper columns that the zero-writer pixel repeater must clear.
- `zero_writer`: separate local zero-fill path, interleaved with real DMA loads.
  Checks partial SPAD/Accum masks, block strides, preserved neighboring values,
  skipped scaling, and final-row byte accounting. Also runs with DIM=8 and 4-byte beats.
  A dependent store separates the poison load from its overlapping zero fill:
  the independent load branches do not themselves guarantee same-address ordering.
- `loop_conv_padding`: a 1x1 image with one input channel, a 2x2 kernel,
  two output channels, and bias. One pixel of padding on all four sides
  produces a 2x2 output. Checks zero-fill of eight initially nonzero SPAD
  rows, real pixel/weight placement, independently computed accumulator
  results, DRAM output, completions, and loop retirement.
- `loop_conv_images`: two distinct 1x4 images with four input channels and
  two output channels run back-to-back without reset. Both use the same
  1x1 kernel, bias, and SPAD A/B locations, but separate DRAM destinations.
  Checks both numerical results, final SPAD contents, completion tags,
  and loop/utilization drain.

**Memory-path checks**

- `full_width_accum_load`: a four-element, 32-bit accumulator row loaded from two DRAM beats.
- `accum_scale_store`: loads signed full-width accumulator rows and stores half-scaled, ReLU, and original full-width values. Checks rounding, clipping, completion tags, and DRAM bytes in DIM4, DIM8, and four-byte memory builds.
- `dim8_loads`: separate `dim=8` build; full and partial SPAD/Accum row loads.

**Cross-controller interaction**

- `shared_accum_read`: ExCtrl reads accumulator D while Store reads another accumulator tile; checks C, DRAM output, and both completion paths.

Accumulator responses use `AccScaleFinite` after Normalizer and StScaleCtrl.
`SmeshConfig::acc_scale_lanes` selects the total lanes (currently eight: four
normalization-capable and four ordinary); `acc_scale_latency` selects pipe
stages after arbOut (currently four). Completed rows go to StIssueCtrl for
stores or AccumExResp for ExCtrl, according to `from_dma`. Softmax remains deferred.
The runner waits for expected DRAM bytes as well as RS completion before its
final drain countdown: Store completion can precede scaling and memory writes.

## Performance measurements

The `tb_rs_to_mem_suite` runner and its variants measure how many simulation
cycles a test case takes and how many commands Smesh accepts at its external
input. Every variant prints a `[SMESH_PERF]` summary by default. These are
end-to-end measurements, including waits and overlapping load, execute, and
store work. They do not measure useful mesh MAC cycles.

### How measurements are collected

After each `Sim::run()`, the runner asks the harness two questions: was an input
command accepted, and is the test's activity complete? It passes these two
answers to `RsMemPerfStats::sample()` in `tb_rs_to_mem_perf.cpp`. This helper
updates totals using the [shared performance counters](../../support/perf/README.md).
The shared counters store and report values; this suite's monitor defines when
to count them.

Input acceptance means the command driver's `cmd_valid && cmd_ready`. These are
commands at Smesh's external input, including `CONFIG`, `PRELOAD`, and loop
macro commands. Commands generated internally by LoopConv or LoopMatmul are
not counted separately here.

Completion means the harness's `activityComplete()` condition: command driver
finished, RS empty, expected DRAM results visible, pending memory writes
finished, and any required loop retirement completed. The numerical PASS/FAIL
checks remain separate. Reset and memory-image setup happen before sampling
begins. All measurement code lives under `tests/`; the functional Smesh
components contain no added measurement ports or counters.

### Measurement definitions

| Name | Meaning |
| --- | --- |
| `simulation_cycles` | Number of sampled `Sim::run()` calls after reset/setup. |
| `startup_cycles` | Cycles before the first input command is accepted. |
| `execution_cycles` | First accepted input command through the first cycle of the final uninterrupted completion period, including both endpoints. |
| `drain_cycles` | Additional sampled cycles after that completion cycle. |
| `input_commands` | Accepted commands at the external Smesh input, including configuration and loop macro commands. |
| `execution_window` | `not_started`, `incomplete`, or `complete`; independent of the numerical PASS/FAIL result. |

`startup_cycles + execution_cycles + drain_cycles == simulation_cycles`.
If activity resumes during drain, the earlier completion boundary is cancelled;
the provisional drain cycles become part of execution. A timeout before completion
reports an incomplete execution window. With no accepted command, all sampled
cycles are startup and the window is `not_started`.

The test case's `drain_cycles` setting counts consecutive completion checks,
including the first completion cycle. A setting of 16 therefore produces 15
additional drain cycles in the report. The monitor follows the runner's
existing termination rule.

### Output and tracing

Use `-perf=0` to hide the summary. Counting continues regardless of this option
or trace selection. From the repository root:

```bash
cmake --build build --target tb_rs_to_mem_suite tb_rs_to_mem_suite_first_layer -j
./build/smesh/tb_rs_to_mem_suite -test=loop_conv_temporal3
./build/smesh/tb_rs_to_mem_suite_first_layer -test=loop_conv_temporal3_pixel2
./build/smesh/tb_rs_to_mem_suite -test=basic -trace '*/rs_mem_perf_'
```

The optional `rs_mem_perf_` trace shows input acceptance, the completion condition,
and execution/drain totals each cycle. Its explicit cycle number starts at 1;
Cascade's timestamp prefix is simulation time in nanoseconds. The runner
flushes Descore output before exiting.

### Example comparison

For the unpadded 1x6/1x3 convolution in the current DIM4 model:

| Mode | Execution cycles | Extra drain cycles | Total simulation cycles |
| --- | ---: | ---: | ---: |
| Repetition disabled | 123 | 15 | 138 |
| Two-pixel repetition | 126 | 15 | 141 |

Both accept nine external commands and have zero startup cycles. These are
end-to-end test-case times; they do not identify the reason for the difference.

## Two-pixel convolution packing

`tb_rs_to_mem_suite_first_layer` links `smesh_model_first_layer`, which enables
`has_first_layer_optimizations`. The normal suite keeps this feature disabled.
Both new cases explicitly request two pixels in the `LOOP_CONV_WS` launch.

Each pair below is one input position's two channels. Repetition packs the next
position into the upper two columns of the preceding SPAD row:

| SPAD row | Unpadded input | Padded input |
| --- | --- | --- |
| 0 | `1, 2, 2, -1` | `0, 0, 1, 2` |
| 1 | `2, -1, 0, 3` | `1, 2, 2, -1` |
| 2 | `0, 3, -2, 1` | `2, -1, 0, 3` |
| 3 | `-2, 1, 3, 2` | `0, 3, -2, 1` |
| 4 | `3, 2, 1, -2` | `-2, 1, 3, 2` |
| 5 | `1, -2, 0, 0` | `3, 2, 1, -2` |
| 6 | unused | `1, -2, 0, 0` |
| 7 | unused | `0, 0, 23, -17` |

The padded row 7's upper columns deliberately retain poison: no generated
compute reads them. The zero-writer repeater must clear row 6's upper columns
and row 7's lower columns. The left border's shifted copy is suppressed at
SPAD row zero. Weight rows 26-31 retain their original two-column layout.

Each table row below represents a PRELOAD followed by a COMPUTE. Every PRELOAD
targets the indicated accumulator rows with accumulation enabled. Shapes are
rows x columns; a garbage B operand reuses the preceding group's weights.

| Case | B operand for PRELOAD | A operand for COMPUTE | C destination | COMPUTE |
| --- | --- | --- | --- | --- |
| Unpadded | `spad[26], 4x2` | `spad[0], 4x4` | `acc[0], 4x2` | FLIP |
| Unpadded | `spad[30], 2x2` | `spad[2], 4x2` | `acc[0], 4x2` | FLIP |
| Padded | `spad[26], 4x2` | `spad[0], 4x4` | `acc[0], 4x2` | FLIP |
| Padded | `garbage, 4x2` | `spad[4], 2x4` | `acc[4], 2x2` | STAY |
| Padded | `spad[30], 2x2` | `spad[2], 4x2` | `acc[0], 4x2` | FLIP |
| Padded | `garbage, 2x2` | `spad[6], 2x2` | `acc[4], 2x2` | STAY |

Thus taps 0-1 are computed together and tap 2 separately. Expected output pairs
are `[[2,11],[1,-11],[16,2],[6,0]]` without padding, and
`[[9,-4],[2,11],[1,-11],[16,2],[6,0],[7,3]]` with padding.
The checker computes these values directly from the input, weights, and bias.

RS load ranges include the preceding rows written by pixel repetition and
retain the original final row. Execute dependencies on any of those rows are
released only when the load completes.

```bash
cmake --build build --target tb_rs_to_mem_suite_first_layer -j
ctest --test-dir build -R 'temporal3.*pixel2' --output-on-failure
./build/smesh/tb_rs_to_mem_suite_first_layer -test=loop_conv_temporal3_pixel2 -trace '*/rs_mem_conv_execute_'
./build/smesh/tb_rs_to_mem_suite_first_layer -test=loop_conv_temporal3_padded_pixel2 -trace '*/rs_mem_conv_execute_;*MvinPixelRepeater*;*ZeroWriter*'
```

## Current size contract

The tested configuration is `dim=4`, 8-bit elements, 32-bit accumulator
elements, and an 8-byte memory beat. Compile-time checks require the configured
element widths to match `Elem`/`Acc`, the beat width to match the current 64-bit
`smem` interface, and `dim <= 8` for the remaining ExCtrl writeback adapters.
The `dim8_loads` target separately compiles Smesh at `dim=8` and checks all
eight lanes plus six-lane partial overwrites. Larger dimensions remain
unsupported by the full system. `dma_max_bytes` is a transfer limit, not the
beat width.

## Run

From the repository root:

```bash
cmake --build build --target tb_rs_to_mem_suite -j
./build/smesh/tb_rs_to_mem_suite -list_tests
./build/smesh/tb_rs_to_mem_suite -test=basic
./build/smesh/tb_rs_to_mem_suite -test=mul_pre
./build/smesh/tb_rs_to_mem_suite -test=loop_ws_dma_i2
./build/smesh/tb_rs_to_mem_suite -test=loop_ws_dma_k2
./build/smesh/tb_rs_to_mem_suite -test=loop_conv_dma
./build/smesh/tb_rs_to_mem_suite -test=loop_conv_kernel2
./build/smesh/tb_rs_to_mem_suite -test=loop_conv_temporal3
./build/smesh/tb_rs_to_mem_suite -test=loop_conv_temporal3_2x6
./build/smesh/tb_rs_to_mem_suite -test=loop_conv_temporal3_padded
./build/smesh/tb_rs_to_mem_suite -test=loop_conv_padding
./build/smesh/tb_rs_to_mem_suite -test=loop_conv_images
./build/smesh/tb_rs_to_mem_suite -test=full_width_accum_load
./build/smesh/tb_rs_to_mem_suite -test=shared_accum_read
./build/smesh/tb_rs_to_mem_suite -test=accum_scale_store
cmake --build build --target tb_rs_to_mem_suite_dim8 -j
./build/smesh/tb_rs_to_mem_suite_dim8 -test=dim8_loads
ctest --test-dir build -L rs_to_mem --output-on-failure
```

Use `-trace` with the suite executable to inspect production component traces.
The harness also checks expected completion tags, final Accum values, and
configured Spad bank-concurrency bounds.

Trace the convolution workers and memory traffic with:

```bash
./build/smesh/tb_rs_to_mem_suite -test=loop_conv_dma -trace '*LoopConv*;*DmaReader;*DmaWriter'
```
