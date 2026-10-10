"""Generate smfft test vectors and twiddle tables (doc/fixed-point-spec.md, section 7).

Usage:  python3 gen_vectors.py [--B_in 13] [--B_w 16] [--out ../tests/vectors]

Each case directory gets meta.txt, in.txt (I Q per line) and out.txt
(RE IM per line, bit-reversed order). Also writes twiddle tables and an
SQNR summary (fixed-point output vs float DFT).

Sebastian Claudiusz Magierowski Oct 10 2026
"""

import argparse
import cmath
import math
import os
import random

from sdf_fixed import Params, fft_stream, twiddle_table
from sdf_float import sqnr_db


def full_scale(p):
    return (1 << (p.B_in - 1)) - 1


def clamp_in(v, p):
    M = 1 << (p.B_in - 1)
    return min(max(v, -M), M - 1)


def quantise(z, p):
    return (clamp_in(round(z.real), p), clamp_in(round(z.imag), p))


# ----------------------------------------------------------- input cases ---

def impulse(p, frames=1):
    A = full_scale(p)
    return [(A, 0) if t % p.N == 0 else (0, 0) for t in range(frames * p.N)]


def random_fs(p, frames=1, seed=1):
    rng = random.Random(seed)
    M = 1 << (p.B_in - 1)
    return [(rng.randrange(-M, M), rng.randrange(-M, M)) for _ in range(frames * p.N)]


def tone(p, k, frames=1, level=0.9):
    A = level * full_scale(p)
    return [quantise(A * cmath.exp(2j * math.pi * k * t / p.N), p) for t in range(frames * p.N)]


def constant(p, v, frames=1):
    return [v] * (frames * p.N)


def chirp_echo(p, seed=2):
    """Scenario-like frame: three LFM echoes in a 6,900-sample window, zero padded.

    f_s = 180 MHz, tau = 5 us (900 samples), B = 150 MHz.
    """
    fs, tau, B = 180e6, 5e-6, 150e6
    Lc = int(round(tau * fs))
    K = B / tau
    chirp = [cmath.exp(1j * math.pi * K * (t / fs - tau / 2) ** 2) for t in range(Lc)]
    echoes = [(200, 0.5), (3000, 0.3), (5800, 0.15)]      # (start sample, amplitude)
    rng = random.Random(seed)
    A = full_scale(p)
    frame = []
    for t in range(p.N):
        z = 0j
        if t < 6900:
            for start, amp in echoes:
                if start <= t < start + Lc:
                    z += amp * A * chirp[t - start]
            z += complex(rng.uniform(-2, 2), rng.uniform(-2, 2))   # a little noise
        frame.append(quantise(z, p))
    return frame


# ----------------------------------------------------------------- write ---

def write_case(root, name, x, p, desc):
    frames = len(x) // p.N
    y = fft_stream(x, p)
    d = os.path.join(root, name)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "meta.txt"), "w") as f:
        f.write(f"N={p.N}\nB_in={p.B_in}\nB_w={p.B_w}\nframes={frames}\n"
                f"out_width={p.out_width}\ndescription={desc}\n")
    with open(os.path.join(d, "in.txt"), "w") as f:
        f.writelines(f"{i} {q}\n" for i, q in x)
    with open(os.path.join(d, "out.txt"), "w") as f:
        f.writelines(f"{r} {m}\n" for r, m in y)
    s = sqnr_db(y[:p.N], x[:p.N], p.n)
    print(f"  {name:28s} frames={frames}  SQNR(frame 0) = {s:6.1f} dB")
    return s


def write_twiddles(root, N, B_w):
    path = os.path.join(root, f"twiddles_N{N}_W{B_w}.txt")
    with open(path, "w") as f:
        f.write("# d k w_re w_im   (k = 0 is never multiplied and is omitted)\n")
        d = N // 2
        while d >= 1:
            for k, w in enumerate(twiddle_table(d, B_w)):
                if k:
                    f.write(f"{d} {k} {w[0]} {w[1]}\n")
            d //= 2
    print(f"  {os.path.basename(path)}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--B_in", type=int, default=13)
    ap.add_argument("--B_w", type=int, default=16)
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "tests", "vectors"))
    a = ap.parse_args()
    root = os.path.abspath(a.out)
    os.makedirs(root, exist_ok=True)

    print(f"Writing vectors to {root} (B_in={a.B_in}, B_w={a.B_w})")
    small = lambda N: Params(N=N, B_in=a.B_in, B_w=a.B_w)
    p8, p16, p64, p1k, p8k = small(8), small(16), small(64), small(1024), small(8192)
    M = 1 << (a.B_in - 1)

    write_case(root, "n8_impulse", impulse(p8), p8, "impulse at t=0")
    write_case(root, "n8_random", random_fs(p8, 2), p8, "full-scale uniform random, 2 frames")
    write_case(root, "n16_random", random_fs(p16, 2), p16, "full-scale uniform random, 2 frames")
    write_case(root, "n64_tone_k5", tone(p64, 5), p64, "complex tone in bin 5, 0.9 FS")
    write_case(root, "n64_random", random_fs(p64, 2), p64, "full-scale uniform random, 2 frames")
    write_case(root, "n1024_random", random_fs(p1k), p1k, "full-scale uniform random")
    write_case(root, "n8192_tone_k1", tone(p8k, 1), p8k, "complex tone in bin 1, 0.9 FS")
    write_case(root, "n8192_tone_k3001", tone(p8k, 3001), p8k, "complex tone in bin 3001, 0.9 FS")
    write_case(root, "n8192_random", random_fs(p8k, 2), p8k, "full-scale uniform random, 2 frames")
    write_case(root, "n8192_maxpos", constant(p8k, (M - 1, M - 1)), p8k, "all samples (+max, +max): DC overflow check")
    write_case(root, "n8192_minneg", constant(p8k, (-M, -M)), p8k, "all samples (-M, -M): most negative input")
    write_case(root, "n8192_chirp_echo", chirp_echo(p8k), p8k,
               "3 LFM echoes (180 MHz, 5 us, 150 MHz) in 6,900 samples, zero padded")
    for N in (8, 8192):
        write_twiddles(root, N, a.B_w)

    # SQNR sweep (not written as vectors): how input and twiddle widths matter
    print("SQNR sweep, N=8192, random full-scale input (dB):")
    print("  B_in \\ B_w " + "".join(f"{w:>8d}" for w in (12, 14, 16, 18)))
    for b in (10, 13, 16):
        row = []
        for w in (12, 14, 16, 18):
            p = Params(N=8192, B_in=b, B_w=w)
            x = random_fs(p, 1)
            row.append(sqnr_db(fft_stream(x, p), x, p.n))
        print(f"  {b:>4d}       " + "".join(f"{s:8.1f}" for s in row))


if __name__ == "__main__":
    main()
