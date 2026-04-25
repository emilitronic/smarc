# smystolic

Cascade-native systolic array experiments for smarc.

This directory is intentionally starting small. The first goal is to develop a
stand-alone systolic array module with simple testbenches before coupling it to
`smile` or `smicro`.

Initial development priorities:

- Keep interfaces explicit and narrow.
- Separate compute fabric, control flow, and memory/test harness code.
- Prefer small deterministic tests before adding randomized delay behavior.
- Add complexity only when the previous behavior is easy to explain and verify.
