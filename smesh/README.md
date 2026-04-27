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