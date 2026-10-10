"""Bit-accurate fixed-point golden model of the smfft radix-2 DIF SDF FFT.

Implements doc/fixed-point-spec.md (v0.1). Two equivalent forms are provided:

  fft_frame()   frame-at-a-time: applies the spec stage by stage to a whole frame
  SdfStream     cycle-by-cycle: one sample in, one sample out per step, with a
                d-word delay line per stage, as the hardware does it

self_test() checks that the two produce identical integers, and that both agree
with a floating-point DFT to within quantisation error.

Sebastian Claudiusz Magierowski Oct 10 2026
"""

from collections import deque
from dataclasses import dataclass
import math


@dataclass(frozen=True)
class Params:
    N: int = 8192      # FFT size
    B_in: int = 13     # input width (each of I, Q)
    B_w: int = 16      # twiddle width (each of re, im)

    def __post_init__(self):
        assert self.N >= 2 and self.N & (self.N - 1) == 0, "N must be a power of 2"
        assert 2 <= self.B_in <= 16, "B_in out of range"
        assert 10 <= self.B_w <= 24, "B_w out of range"

    @property
    def n(self):
        return self.N.bit_length() - 1

    def width(self, s):
        """Word width after stage s (s = 0 means stage-1 input)."""
        return self.B_in + 1 + s

    @property
    def out_width(self):
        return self.width(self.n)


# ---------------------------------------------------------------- helpers --

def bitrev(m, n):
    r = 0
    for _ in range(n):
        r = (r << 1) | (m & 1)
        m >>= 1
    return r


def round_half_away(x):
    return int(math.copysign(math.floor(abs(x) + 0.5), x))


def twiddle(d, k, B_w):
    """Quantised W_{2d}^k as (w_re, w_im), spec section 5.1 (k >= 1)."""
    S = 1 << (B_w - 1)
    theta = math.pi * k / d
    clamp = lambda v: min(max(v, -S), S - 1)
    return clamp(round_half_away(math.cos(theta) * S)), clamp(round_half_away(-math.sin(theta) * S))


def twiddle_table(d, B_w):
    """Twiddles for one stage, k = 1 .. d-1 (index 0 unused: multiplier bypassed)."""
    return [None] + [twiddle(d, k, B_w) for k in range(1, d)]


def rot(v, w, B_w):
    """Complex multiply and round half up, spec section 5.2."""
    vr, vi = v
    wr, wi = w
    sh = B_w - 1
    half = 1 << (sh - 1)
    return ((vr * wr - vi * wi + half) >> sh, (vr * wi + vi * wr + half) >> sh)


def check_width(v, W, where):
    lo, hi = -(1 << (W - 1)), (1 << (W - 1)) - 1
    assert lo <= v[0] <= hi and lo <= v[1] <= hi, f"overflow at {where}: {v} not in {W} bits"


def check_input(x, p):
    for t, v in enumerate(x):
        check_width(v, p.B_in, f"input sample {t}")


# ------------------------------------------------------ frame-at-a-time ----

def fft_frame(x, p, tables=None):
    """One frame of N (I, Q) integer pairs -> N (re, im) outputs in bit-reversed order."""
    assert len(x) == p.N
    check_input(x, p)
    tables = tables or {}
    a = list(x)
    for s in range(1, p.n + 1):
        d = p.N >> s
        tw = tables.get(d) or tables.setdefault(d, twiddle_table(d, p.B_w))
        for base in range(0, p.N, 2 * d):
            for i in range(d):
                u, v = a[base + i], a[base + i + d]
                sm = (u[0] + v[0], u[1] + v[1])
                df = (u[0] - v[0], u[1] - v[1])
                if i:
                    df = rot(df, tw[i], p.B_w)
                check_width(sm, p.width(s), f"stage {s} sum")
                check_width(df, p.width(s), f"stage {s} diff")
                a[base + i], a[base + i + d] = sm, df
    return a   # in-place DIF order == SDF output order == bit-reversed bins


def fft_stream(x, p):
    """Any number of whole frames, back to back."""
    assert len(x) % p.N == 0
    tables = {}
    out = []
    for f in range(0, len(x), p.N):
        out += fft_frame(x[f:f + p.N], p, tables)
    return out


# ------------------------------------------------------ cycle-by-cycle -----

class SdfStage:
    """One radix-2 DIF SDF stage: d-word delay line + butterfly + output rotator."""

    def __init__(self, d, p):
        self.d, self.p = d, p
        self.tw = twiddle_table(d, p.B_w)
        self.dl = deque([(0, 0)] * d)
        self.cnt = 0

    def step(self, x):
        c = self.cnt % (2 * self.d)
        y = self.dl.popleft()
        if c < self.d:                       # fill: store input, release stored difference
            self.dl.append(x)
            if c:
                y = rot(y, self.tw[c], self.p.B_w)
        else:                                # butterfly: sum out, raw difference stored
            self.dl.append((y[0] - x[0], y[1] - x[1]))
            y = (y[0] + x[0], y[1] + x[1])
        self.cnt += 1
        return y


class SdfStream:
    """Chain of n stages. Latency N-1 steps (no pipeline registers)."""

    def __init__(self, p):
        self.p = p
        self.stages = [SdfStage(p.N >> s, p) for s in range(1, p.n + 1)]

    def step(self, x):
        for st in self.stages:
            x = st.step(x)
        return x

    def run(self, x):
        """Feed whole frames, flush with zeros, return the aligned output stream."""
        check_input(x, self.p)
        lat = self.p.N - 1
        out = [self.step(v) for v in list(x) + [(0, 0)] * lat]
        return out[lat:]


# ------------------------------------------------------------ self test ----

def sqnr_db(out_fixed, x, p):
    """SQNR of the fixed-point output vs. the exact float DFT of the same integers."""
    from sdf_float import sqnr_db as _sqnr
    return _sqnr(out_fixed, x, p.n)


def self_test():
    import random
    rng = random.Random(1)
    for N in (2, 4, 8, 16, 64, 256):
        p = Params(N=N, B_in=13, B_w=16)
        M = 1 << (p.B_in - 1)
        x = [(rng.randrange(-M, M), rng.randrange(-M, M)) for _ in range(3 * N)]
        a = fft_stream(x, p)
        b = SdfStream(p).run(x)
        assert a == b, f"frame and stream models differ at N={N}"
        assert sqnr_db(a[:N], x[:N], p) > 60, f"poor SQNR at N={N}"
    print("self_test passed: frame == stream (bit-exact) for N = 2..256, SQNR > 60 dB")


if __name__ == "__main__":
    self_test()
