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

The direct-command scenarios issue `CONFIG_EX`, `PRELOAD`, and `COMPUTE_*`.
The `loop_ws` scenario starts with Spad data already loaded. The `loop_ws_dma`
scenarios instead seed A, B, and D in DRAM and exercise generated loads,
execute commands, and stores through the shared memory boundary. The harness
checks completions and Accum results, plus loaded Spad rows and final DRAM
bytes for the DMA scenarios.

Commands now pass through `SmeshCmdQueue -> LoopConv -> LoopMatmul ->
SmeshUnrolledCmdQueue -> SmeshRS`. Each unroller receives credits only for its
own generated commands. Load/Store CONFIG credits return on RS issue;
ordinary command credits return on controller completion.

The suite currently covers:

- `basic`: PRELOAD followed by COMPUTE_FLIP and COMPUTE_STAY.
- `mul_pre`: two operations using the overlapping COMPUTE+PRELOAD path.
- `concurrent_banks`: checks that Spad reads can fire on three banks at once.
- `same_bank_serializes`: checks that aliased A/addend reads serialize.
- `loop_ws`: one tile with operands initialized in Spad.
- `loop_ws_dma`: one tile loaded from DRAM and stored back to DRAM.
- `loop_ws_dma_i2`: two output tiles with a shared B tile.
- `loop_ws_dma_k2`: two products accumulated into one output tile.
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
- `full_width_accum_load`: a four-element, 32-bit accumulator row loaded from two DRAM beats.
- `shared_accum_read`: ExCtrl reads accumulator D while Store reads another accumulator tile; checks C, DRAM output, and both completion paths.
- `accum_scale_store`: loads signed full-width accumulator rows and stores half-scaled, ReLU, and original full-width values. Checks rounding, clipping, completion tags, and DRAM bytes in DIM4, DIM8, and four-byte memory builds.
- `dim8_loads`: separate `dim=8` build; full and partial SPAD/Accum row loads.

The focused memory unit tests cover bank-local arbitration and simultaneous
operations independently. This suite also checks Spad read concurrency and
same-bank serialization through the full command-to-memory composition.

Accumulator responses use `AccScaleFinite` after Normalizer and StScaleCtrl.
`SmeshConfig::acc_scale_lanes` selects the total lanes (currently eight: four
normalization-capable and four ordinary); `acc_scale_latency` selects pipe
stages after arbOut (currently four). Completed rows go to StIssueCtrl for
stores or AccumExResp for ExCtrl, according to `from_dma`. Softmax remains deferred.
The runner waits for expected DRAM bytes as well as RS completion before its
final drain countdown: Store completion can precede scaling and memory writes.

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
