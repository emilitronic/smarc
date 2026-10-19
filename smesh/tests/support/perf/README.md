# Smesh performance measurement

These files support simulation measurements independently of the functional
hardware model. The first observer is used by every `rs_to_mem` runner variant.

## Organization

- `SmeshPerfEvents.hpp`: the shared list of measurement names.
- `SmeshPerfCounters.hpp/cpp`: exact 64-bit totals and Descore log reporting.
- `../../integration/rs_to_mem/tb_rs_to_mem_perf.hpp/cpp`: the observer that
  determines the execution window from the input handshake and harness completion.

The runner calls the observer once after each `Sim::run()`. Input acceptance is
the command driver's `cmd_valid && cmd_ready` for that simulated cycle. Reset
and memory-image setup happen before sampling begins. There are no added hardware
ports or counters in the functional Smesh components.

## Measurement definitions

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

Completion means the existing harness's `activityComplete()` condition: command
driver finished, RS empty, expected DRAM results visible, pending memory writes
finished, and any required loop retirement completed. This measures the whole
scenario, including waits and overlapping load/execute/store work. It does not
measure useful mesh MAC cycles or replace the numerical checker.

The runner's `drain_cycles` scenario setting counts consecutive completion checks,
including the first completion cycle. A setting of 16 therefore produces 15
additional drain cycles. This observer preserves the existing termination rule.

## Output and tracing

Performance summaries are enabled by default; use `-perf=0` to suppress them.
Accumulation is independent of both the report flag and trace selection.

```bash
cmake --build build --target tb_rs_to_mem_suite tb_rs_to_mem_suite_first_layer -j
./build/smesh/tb_rs_to_mem_suite -test=loop_conv_temporal3
./build/smesh/tb_rs_to_mem_suite_first_layer -test=loop_conv_temporal3_pixel2
./build/smesh/tb_rs_to_mem_suite -test=basic -trace '*/rs_mem_perf_'
ctest --test-dir build -R '^smesh_unit_perf$' --output-on-failure
```

The optional `rs_mem_perf_` trace shows the input handshake, completion condition,
and execution/drain totals each cycle. Its explicit cycle number starts at 1;
Cascade's timestamp prefix is simulation time in nanoseconds.

Totals use integer Descore logging instead of `Statistics::recordStat()`, whose
float argument can lose precision for large cycle or byte totals. The runner
flushes Descore output before exiting.

## Adding measurements

Add an event and its report name together in `SmeshPerfEvents.hpp`, then record it
from the observer that can see its signals. Use `add()` for event occurrences or
byte increments and `set()` for totals derived from measurement boundaries.
Other events may overlap in a cycle; only startup/execution/drain partition time.
When adopting an Original event, preserve and document its actual predicate:
for example, `EXE_ACTIVE_CYCLE` denotes ExCtrl's compute state, not useful MACs.

Verify a new observation condition with a known event sequence, especially its
behavior during stalls. `tb_smesh_perf` currently checks inclusive boundaries,
resumed activity, incomplete runs, reset, and integer totals above float precision.
