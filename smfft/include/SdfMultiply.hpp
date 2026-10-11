// **********************************************************************
// smfft/include/SdfMultiply.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
The stage rotator: complex multiply then round, per doc/fixed-point-spec.md
section 5.2. Pure code, no Cascade component. Mirrors rot() in
python/sdf_fixed.py statement for statement.

    P_re = v_re*w_re - v_im*w_im        exact, before any rounding
    P_im = v_re*w_im + v_im*w_re
    out  = (P + 2^(B_w-2)) >> (B_w-1)

Two things about this that are easy to get wrong:

Rounding is round half UP (toward +infinity), not the round_half_away used to
quantise the twiddles in SdfTwiddle.hpp. The two agree on positive values and
disagree on every negative exact half: -2.5 rounds to -2 here and to -3 there.
That is not an inconsistency in the spec; one is a hardware datapath (add half
an LSB, truncate) and the other is an offline constant generator.

The products are formed exactly first, which is what lets a 3-multiplier
(Karatsuba-style) and a 4-multiplier implementation give bit-identical
results. Round each product separately and that guarantee is gone.

Width: the widest legal case is a 30-bit value times a 24-bit twiddle, so a
product reaches 53 bits and the sum of two reaches 54. int64_t carries that
with room; int32_t would silently wrap at the real target (27-bit values at
N=8192), so the intermediates are int64_t deliberately and the narrowing back
to int32_t happens only after the shift.
*/
#pragma once

#include "SdfTypes.hpp"

#include <cstddef>
#include <cstdint>

namespace smfft {

// Round half up by one shift: add half an LSB, then floor.
//
// The >> is an arithmetic (sign-propagating) shift, which is what gives the
// floor on negative values and what the hardware does. C++17 leaves the sign
// behaviour of >> on negative operands implementation-defined (C++20 mandates
// arithmetic), so tb_sdf_multiply checks the property once rather than trusting
// it silently.
inline std::int64_t roundShift(std::int64_t p, std::size_t B_w) {
  const std::size_t sh   = B_w - 1;
  const std::int64_t half = std::int64_t{1} << (sh - 1);
  return (p + half) >> sh;
}

// v * w, rounded back to the width of v. This is Python's rot().
//
// Called only where the stage counter is non-zero: at count 0 the twiddle is
// W^0 = 1 and the multiplier is bypassed entirely (spec section 4), so this
// function is never asked to reproduce an exact identity.
inline SdfComplex rotate(const SdfComplex& v, const SdfComplex& w, std::size_t B_w) {
  const std::int64_t v_re = v.re;
  const std::int64_t v_im = v.im;
  const std::int64_t w_re = w.re;
  const std::int64_t w_im = w.im;

  const std::int64_t p_re = v_re * w_re - v_im * w_im;
  const std::int64_t p_im = v_re * w_im + v_im * w_re;

  SdfComplex out{};
  out.re = static_cast<std::int32_t>(roundShift(p_re, B_w));
  out.im = static_cast<std::int32_t>(roundShift(p_im, B_w));
  return out;
}

} // namespace smfft
