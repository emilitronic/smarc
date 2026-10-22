# Shared performance counters

This README describes the reusable counters that Smesh testbenches use to
record performance measurements, such as clock cycles and accepted commands.
The counter code stores and reports totals. Each testbench supplies the code
that watches simulation signals and decides when to update those totals.

## Organization

- `SmeshPerfEvents.hpp`: the shared list of measurement names.
- `SmeshPerfCounters.hpp/cpp`: exact 64-bit totals and Descore log reporting.

These files have no dependency on a particular test runner or harness. The
current measurement names are `simulation_cycles`, `startup_cycles`,
`execution_cycles`, `drain_cycles`, and `input_commands`. The testbench that
records them defines the exact counting conditions.

## Using the counters

`SmeshPerfCounters` provides five operations:

- `reset()`: clear every total.
- `add(event, amount)`: increment a total; `amount` defaults to one.
- `set(event, value)`: assign a total calculated by the testbench.
- `value(event)`: read a total.
- `report(name)`: print the totals with a name identifying the measured run.

For example, testbench code can count one observed cycle and an accepted command:

```cpp
counters.add(smesh::tb::SmeshPerfEvent::SimulationCycles);
if (command_accepted) {
  counters.add(smesh::tb::SmeshPerfEvent::InputCommands);
}
```

The testbench decides where to call this code and how to determine
`command_accepted`. A helper that watches signals and updates counters is called
an **observer**. Such helpers belong with the tests whose signals and completion
rules they understand.

Totals use integer Descore logging instead of `Statistics::recordStat()`, whose
float argument can lose precision for large cycle or byte totals. The caller
should flush Descore output before exiting.

## Existing use

The `rs_to_mem` integration suite uses these counters through
`tb_rs_to_mem_perf.hpp/cpp`, located alongside that suite. Its
[performance documentation](../../integration/rs_to_mem/README.md#performance-measurements)
defines the sampling points, execution interval, completion rules, and report
options. Other testbenches can use the shared counters with their own observation
code.

## Adding measurements

Add an event and its report name together in `SmeshPerfEvents.hpp`, then record it
from testbench code that can see the relevant signals. Use `add()` for event occurrences or
byte increments and `set()` for totals derived from measurement boundaries.
Document the counting condition alongside that testbench's measurement code.
When adopting an Original event, preserve and document its actual predicate:
for example, `EXE_ACTIVE_CYCLE` denotes ExCtrl's compute state, not useful MACs.

Verify a new observation condition with a known event sequence, especially its
behavior during stalls. `tb_smesh_perf` currently checks inclusive boundaries,
resumed activity, incomplete runs, reset, and integer totals above float precision.

Run those checks from the repository root:

```bash
cmake --build build --target tb_smesh_perf -j
ctest --test-dir build -R '^smesh_unit_perf$' --output-on-failure
```
