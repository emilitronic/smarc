# smfft

`smfft` is the planned FFT accelerator model for smarc: a streaming radix-2 DIF
single-path delay feedback (SDF) FFT.

## Background

`doc/fixed-point-spec.md` defines the arithmetic exactly: input and twiddle
widths, word growth, the stage operation, twiddle quantisation, the multiply's
rounding, and the output order. The C++ model must produce the same output
integers as the Python model for every test vector. Change the spec first if
either side needs to change.

Output timing (latency, pipeline registers, handshake) is not part of the
arithmetic contract and is up to the Cascade model.

## Python reference (`python/`)

| File | Role |
|---|---|
| `sdf_fixed.py` | Bit-accurate golden model. `fft_frame` (whole frame) and `SdfStream` (cycle by cycle, one delay line per stage) give identical integers. `python3 sdf_fixed.py` runs the self-test. |
| `sdf_float.py` | Float DFT reference and SQNR measurement. |
| `gen_vectors.py` | Writes `tests/vectors/` and prints an SQNR sweep over input and twiddle widths. |

Regenerate vectors (needs numpy):

```bash
cd smfft/python
python3 gen_vectors.py                 # defaults: B_in=13, B_w=16
python3 gen_vectors.py --B_in 16       # other widths
```

## Test vectors (`tests/vectors/`)

Each case directory holds `meta.txt`, `in.txt` (`I Q` per line) and `out.txt`
(`RE IM` per line, bit-reversed bin order). A model passes when its output
sequence equals `out.txt` line for line. Cases run from N = 8 (easy to trace by
hand) up to N = 8,192 (random, tones, extreme inputs, and a scenario-like frame
of three chirp echoes in 6,900 samples plus zero padding).
`twiddles_N<N>_W<B_w>.txt` hold the canonical twiddle values.

Quiet-corner cases (`n64_quiet_tone_k3`, `n8192_quiet_tone_k777`,
`n8192_lsb_noise`, `n8192_weak_echo`) check the noise floor, where rounding is
a large part of the signal. `tests/vectors_b16_w24/` holds the same cases at the
widest legal widths (B_in = 16, B_w = 24: 30-bit values, 54-bit products).

Suggested order for bringing up the C++ model: `n8_impulse`, `n8_random`,
`n16_random`, `n64_*`, `n1024_random`, then the `n8192_*` cases.

## Status

- v0.1 spec: radix-2 DIF, full growth, round half up on twiddle multiplies.
- Planned: radix-2², per-stage scaling, other rounding modes, ADC/CDC source,
  zero-pad block, memory writer, control/status registers.
