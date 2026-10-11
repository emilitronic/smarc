// **********************************************************************
// smfft/include/SdfTypes.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 10 2026
/*
Shared types for the radix-2 DIF SDF FFT model (doc/fixed-point-spec.md v0.1).

Sample components are plain signed C++ integers rather than Cascade sN
bit-vectors, matching smesh's Elem/Acc convention for signed data carried
inside port structs. The spec works on integers with no binary point, and
the widest legal internal value is B_in + 1 + n bits (30 bits for
B_in = 16, N = 8192), so 32 bits covers every legal configuration.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include <cstddef>
#include <cstdint>

namespace smfft {

// One complex sample. Only the low (B_in + 1 + s) bits are meaningful at
// stage s; the spec guarantees no overflow of that width.
struct SdfComplex {
  std::int32_t re = 0;
  std::int32_t im = 0;
};

inline bool operator==(const SdfComplex& a, const SdfComplex& b) {
  return a.re == b.re && a.im == b.im;
}
inline bool operator!=(const SdfComplex& a, const SdfComplex& b) { return !(a == b); }

// Spec section 1 parameters.
struct SdfParams {
  std::size_t N = 8192;  // FFT size, power of two >= 2
  std::size_t B_in = 13; // input width per I/Q, 2..16
  std::size_t B_w = 16;  // twiddle width per re/im, 10..24

  std::size_t stages() const {
    std::size_t n = 0;
    for (std::size_t v = N; v > 1; v >>= 1) {
      ++n;
    }
    return n;
  }
  // Word width after stage s; s = 0 is the stage-1 input (one guard bit).
  std::size_t width(std::size_t s) const { return B_in + 1 + s; }
  std::size_t outWidth() const { return width(stages()); }
};

} // namespace smfft
