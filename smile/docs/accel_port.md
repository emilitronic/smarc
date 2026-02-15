# Accelerator Instruction Contract v1 (smile + smicro)

## 1) Scope and Design Intent

This document is the single source of truth for v1 accelerator invocation semantics shared by `smile` and `smicro`.

v1 goal:
- Unify instruction-level invocation and completion semantics.
- Keep payload interpretation flexible so each accelerator can define verb-specific meaning for `rd` and/or memory side effects.

## 2) Opcode and Instruction Format

v1 uses RISC-V `CUSTOM-0`:
- `opcode = 0x0B` (`0001011b`)

Instruction format is R-type:
- `funct7 | rs2 | rs1 | funct3 | rd | opcode`

## 3) Verb Selection

v1 verb selection rules:
- `funct3` (3 bits) selects the verb (`ACCEL_*` ID).
- `funct7` must be `0b0000000` for v1.
- Non-zero `funct7` is reserved for future flags/extensions.

## 4) Register ABI (Stable Calling Convention)

- `rs1` = `arg0` (often an address, verb-specific)
- `rs2` = `arg1` (often a length or pointer, verb-specific)
- `rd`  = 32-bit return payload (verb-specific)
- If `rd == x0`, the return payload is discarded.

## 5) Completion Semantics (Blocking)

v1 completion is blocking:
- The core issues the `CUSTOM-0` instruction and stalls until an accelerator response is available.
- Once a response arrives, the core writes the 32-bit payload to `rd` (unless `rd == x0`) and then advances `pc += 4`.
- Accelerators may be single-cycle (response in the issue cycle) or multi-cycle (response after internal work).

## 6) Error Policy (No Trap for Missing/Unsupported Accelerator)

Missing accelerator and unsupported verb are interface-level conditions, not illegal instruction traps in v1.

Required behavior:
- If no accelerator is attached, the core must return immediately with `rd = ACCEL_E_UNSUPPORTED` (unless `rd == x0`) and continue.
- If an accelerator is attached but does not support the requested verb, it must return `ACCEL_E_UNSUPPORTED`.

Shared interface-level error codes:
- `ACCEL_E_UNSUPPORTED = 1`
- `ACCEL_E_BUSY = 2` (reserved)
- `ACCEL_E_BADARG = 3` (reserved)

Contract note:
- These `ACCEL_E_*` codes are only guaranteed for missing/unsupported interface conditions.
- Otherwise, return payload meaning is defined by the selected accelerator verb ABI.

## 7) Payload Conventions

`rd` payload interpretation is verb-specific/accelerator-specific.

Expected patterns:
- Some verbs return a direct numeric result in `rd`.
- Other verbs return status in `rd`, while writing bulk outputs to memory at ABI-defined locations.

Guideline:
- Prefer returning direct scalar results in `rd` when they naturally fit in 32 bits.
- Otherwise return status in `rd` and place larger/structured outputs in memory.

## 8) Initial Verb IDs (`funct3` Map)

| `funct3` | Name | v1 Meaning |
|---|---|---|
| `0b000` | `ACCEL0_ARRAY_SUM_U32` | Existing demo verb |
| `0b001` | `ACCEL_RESET` | Reserved |
| `0b010` | `ACCEL_SET_N` | Reserved |
| `0b011` | `ACCEL_SET_PARAM0` | Reserved |
| `0b100` | `ACCEL_SET_PARAM1` | Reserved |
| `0b101` | `ACCEL_RUN` | Reserved |
| `0b110` | `ACCEL_RESERVED6` | Reserved |
| `0b111` | `ACCEL_RESERVED7` | Reserved |

### `ACCEL0_ARRAY_SUM_U32` (`funct3=0b000`)

- `rs1` = base byte address (expected 4-byte aligned)
- `rs2` = length in 32-bit words
- `rd`  = sum (`uint32_t`)

Notes:
- Overflow wraps modulo `2^32`.
- Alignment violations are accelerator-ABI defined; v1 recommends treating misalignment as `ACCEL_E_BADARG` once that path is implemented.
- Demo implementations may return only the sum (without status multiplexing).

## 9) Example Sequences

### 9.1 Array Sum (Single `CUSTOM-0`)

Conceptual single instruction:
- `verb = ACCEL0_ARRAY_SUM_U32` (`funct3=0b000`)
- `funct7=0`
- `rs1=a0` (base), `rs2=a1` (length), `rd=t0` (sum)

Pseudo sequence:
1. Software sets `a0 = base_addr`, `a1 = word_len`.
2. Execute one `CUSTOM-0` with `funct3=0b000`, `funct7=0`.
3. Core blocks until response; `t0` receives `sum`.

### 9.2 Basecaller-Style Sequence (Multi-Verb Control + `RUN`)

Conceptual command flow:
1. `ACCEL_RESET` (`funct3=0b001`)
2. `ACCEL_SET_N` (`funct3=0b010`)
3. `ACCEL_SET_PARAM0` / `ACCEL_SET_PARAM1` (`funct3=0b011/100`) as needed
4. `ACCEL_RUN` (`funct3=0b101`)

Behavior model:
- `ACCEL_RUN` may be multi-cycle.
- During `RUN`, the core remains blocked until completion response is available.
- Per-accelerator ABI may define:
  - `rd` returns status or end-state code.
  - bulk outputs are written to memory at pre-agreed addresses configured by earlier verbs.

This sequence is a contract pattern only; exact parameter packing and memory layout are accelerator-ABI specific.
