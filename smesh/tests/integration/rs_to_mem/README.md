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

The suite currently covers:

- `basic`: PRELOAD followed by COMPUTE_FLIP and COMPUTE_STAY.
- `mul_pre`: two operations using the overlapping COMPUTE+PRELOAD path.
- `concurrent_banks`: checks that Spad reads can fire on three banks at once.
- `same_bank_serializes`: checks that aliased A/addend reads serialize.
- `loop_ws`: one tile with operands initialized in Spad.
- `loop_ws_dma`: one tile loaded from DRAM and stored back to DRAM.
- `loop_ws_dma_i2`: two output tiles with a shared B tile.
- `loop_ws_dma_k2`: two products accumulated into one output tile.
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
