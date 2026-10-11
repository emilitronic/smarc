// **********************************************************************
// smfft/include/SdfTwiddle.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Twiddle constants for one SDF stage, per doc/fixed-point-spec.md section 5.1.

A table depends only on the stage delay d and the twiddle width B_w; the FFT
size N never enters. The d = 4 table is the same whether it belongs to stage
1 of an 8-point transform or stage 11 of an 8192-point one, which is why the
canonical files are keyed "d k w_re w_im" and why one table may be shared by
every stage of that delay. Forward and inverse use the same constants: the
reference swaps re/im at the chain input and output rather than conjugating
the twiddles (SDFChainRadix2.scala:204-209, :277-282).

The floating-point expressions here deliberately mirror python/sdf_fixed.py
statement for statement, including the order of operations in theta, because
the two models must agree to the last integer. tb_sdf_twiddle checks the
generated tables against tests/vectors/twiddles_N*_W*.txt exactly.
*/
#pragma once

#include "SdfTypes.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace smfft {

// The double nearest pi, same value as Python's math.pi.
constexpr double kSdfPi = 3.14159265358979323846;

// round_half_away(x) = sign(x) * floor(|x| + 0.5)
// Written as the spec writes it rather than via std::round, so that any
// difference in how the halfway case or the addition rounds cannot creep in.
inline std::int64_t roundHalfAway(double x) {
  const double magnitude = std::floor(std::fabs(x) + 0.5);
  return static_cast<std::int64_t>(x < 0.0 ? -magnitude : magnitude);
}

// clamp(x) = min(max(x, -S), S - 1).  Note the asymmetry: +S is not
// representable in B_w bits, so a cosine that rounds up to +S is pulled to
// S - 1. That only happens for small k in large stages.
inline std::int64_t clampTwiddle(std::int64_t x, std::int64_t S) {
  if (x < -S) {
    return -S;
  }
  if (x > S - 1) {
    return S - 1;
  }
  return x;
}

// W_{2d}^k quantised to B_w bits per component, for k >= 1.
inline SdfComplex twiddle(std::size_t d, std::size_t k, std::size_t B_w) {
  const std::int64_t S = std::int64_t{1} << (B_w - 1);
  const double theta = kSdfPi * static_cast<double>(k) / static_cast<double>(d);
  SdfComplex w{};
  w.re = static_cast<std::int32_t>(clampTwiddle(roundHalfAway(std::cos(theta) * static_cast<double>(S)), S));
  w.im = static_cast<std::int32_t>(clampTwiddle(roundHalfAway(-std::sin(theta) * static_cast<double>(S)), S));
  return w;
}

// Table for one stage, indexed by the stage counter k = 0 .. d-1.
//
// Entry 0 is left at {0, 0} rather than holding W^0 = 1. The multiplier is
// bypassed at k = 0 (spec section 4), and +1.0 does not fit in B_w bits
// anyway: computing it would clamp to S - 1, a plausible-looking but wrong
// near-one. A zero makes any code that wrongly multiplies by entry 0 fail
// loudly instead.
inline std::vector<SdfComplex> twiddleTable(std::size_t d, std::size_t B_w) {
  std::vector<SdfComplex> table(d);
  for (std::size_t k = 1; k < d; ++k) {
    table[k] = twiddle(d, k, B_w);
  }
  return table;
}

} // namespace smfft
