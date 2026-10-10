"""Floating-point reference for smfft.

The fixed-point model (sdf_fixed.py) is compared against this to measure
quantisation error (SQNR). It never matches bit for bit; that is the job of
sdf_fixed.py versus the C++ model.

Sebastian Claudiusz Magierowski Oct 10 2026
"""

import math
import numpy as np


def bitrev(m, n):
    r = 0
    for _ in range(n):
        r = (r << 1) | (m & 1)
        m >>= 1
    return r


def dft_bitrev(x, n):
    """Exact float DFT of one frame of (I, Q) integers, returned in bit-reversed order."""
    X = np.fft.fft(np.array([complex(*v) for v in x]))
    return X[[bitrev(m, n) for m in range(len(x))]]


def sqnr_db(out_fixed, x, n):
    """10 log10( signal power / error power ), fixed-point output vs float DFT."""
    ref = dft_bitrev(x, n)
    y = np.array([complex(*v) for v in out_fixed])
    err = np.sum(np.abs(y - ref) ** 2)
    if err == 0:
        return math.inf
    return 10 * math.log10(np.sum(np.abs(ref) ** 2) / err)
