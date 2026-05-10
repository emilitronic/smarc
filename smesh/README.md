# smesh

Cascade-native systolic array experiments for smarc.

This directory is intentionally starting small. The first goal is to develop a
stand-alone systolic array module with simple testbenches before coupling it to
`smile` or `smicro`.

Initial development priorities:

- Keep interfaces explicit and narrow.
- Separate compute fabric, control flow, and memory/test harness code.
- Prefer small deterministic tests before adding randomized delay behavior.
- Add complexity only when the previous behavior is easy to explain and verify.

## Build
Configure from the repository root:
```bash
cmake -S . -B build -DCEDAR_DIR=your/path/to/cedar -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```
Build only the M0 smesh testbench:
```bash
cmake --build build --target tb_smesh_m0 -j
```
Build only the M1 low-level command-surface testbench:
```bash
cmake --build build --target tb_smesh_m1 -j
```
Build only the M2 Cascade command-shell testbench:
```bash
cmake --build build --target tb_smesh_m2 -j
```
Build only the M3 memory-topology scaffold:
```bash
cmake --build build --target tb_smesh_m3 -j
```
Or build all smarc targets:
```bash
cmake --build build -j
```

## Run
Run the M0 functional testbench from the repository root:
```bash
./build/smesh/tb_smesh_m0
```
Expected output:
```text
[SMESH_M0] PASS identity
[SMESH_M0] PASS matmul
```
The M0 testbench is pure C++ and does not use Cascade ports yet. It verifies the
first functional data path:
```text
host memory A/B -> scratchpad -> preload B into PE state -> accumulator C -> host memory
```

Run the M1 low-level command-surface testbench:
```bash
./build/smesh/tb_smesh_m1
```
Expected output:
```text
[SMESH_M1] PASS identity
[SMESH_M1] PASS matmul
```
The M1 testbench drives the same functional data path through decoded
`funct/rs1/rs2` command fields instead of direct method calls. It does not parse
raw RISC-V/RoCC instruction words yet.

Run the M2 Cascade command-shell testbench:
```bash
./build/smesh/tb_smesh_m2
```
Expected output:
```text
[SMESH_M2] PASS identity
[SMESH_M2] PASS matmul
```
The M2 testbench sends the same command stream through Cascade FIFO ports into a
small shell component around `SmeshDevice`. Memory is still functional and owned
by the shell; explicit memory request/response ports are deferred to M3.

Run the M3 memory-topology scaffold:
```bash
./build/smesh/tb_smesh_m3
```
Expected output:
```text
[SMESH_M3] PASS identity
[SMESH_M3] PASS matmul
```
The M3 scaffold wires `SmeshShell` to `smem::MemCtrl` and `smem::Dram` through
native `MemReq`/`MemResp` FIFO ports. At this checkpoint, `SmeshShell` still
executes `mvin`/`mvout` through its functional internal memory; moving those
commands onto the memory ports is the next M3 step.
