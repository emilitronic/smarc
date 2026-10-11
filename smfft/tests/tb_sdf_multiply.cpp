// **********************************************************************
// smfft/tests/tb_sdf_multiply.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Checks the stage rotator against doc/fixed-point-spec.md section 5.2. No
Cascade component, no simulation.

There are no canonical vectors for the multiply on its own -- the vector
directory holds twiddle tables and whole-case in/out pairs -- so the expected
values here are the behavioural spec, written out rather than regenerated.

Three things are being tested, in increasing order of what they would catch:

1. The table: specific (v, w, B_w) -> out triples, including both signs of the
   exact-half case, both clamp-bound twiddles, three B_w values, and a pair of
   extreme inputs whose intermediate product overflows int32.

2. The shift is arithmetic. C++17 leaves >> on a negative signed value
   implementation-defined, so the property is checked rather than assumed; the
   whole rounding rule collapses to something else on a platform that shifts in
   zeroes.

3. 3-multiplier and 4-multiplier structures agree, over many pseudo-random
   inputs. The spec claims this, and it holds only because the products are
   formed exactly before rounding. Rounding each product early -- a plausible
   "optimisation" -- passes the table above and fails this.
*/

#include "SdfMultiply.hpp"
#include "SdfTwiddle.hpp"

#include <cstdio>
#include <cstdint>

namespace {

using namespace smfft;

struct Case {
  SdfComplex  v;
  SdfComplex  w;
  std::size_t B_w;
  SdfComplex  expect;
  const char* what;
};

const Case kCases[] = {
    // Twiddle at the positive clamp bound is very slightly under 1.0, so a
    // value passes through unchanged after rounding.
    {{1000, 0}, {32767, 0}, 16, {1000, 0}, "w = S-1 is near-unity"},

    // w = -j exactly (d=4, k=2, the negative clamp bound). (100+200j)(-j) =
    // 200-100j, exact, no rounding involved.
    {{100, 200}, {0, -32768}, 16, {200, -100}, "w = -j rotates exactly"},

    // The two halves of the rounding rule. Quotient is +-2.5 exactly.
    // Round half UP means +2.5 -> 3 but -2.5 -> -2, magnitudes 3 and 2.
    // round_half_away (used for the twiddles) would give -3 here.
    {{10, 0}, {8192, 0}, 16, {3, 0}, "quotient +2.5 rounds to 3"},
    {{-10, 0}, {8192, 0}, 16, {-2, 0}, "quotient -2.5 rounds to -2, not -3"},

    // Real table entries for d=4.
    {{1000, -2000}, {23170, -23170}, 16, {-707, -2121}, "d=4 k=1"},
    {{1000, -2000}, {0, -32768}, 16, {-2000, -1000}, "d=4 k=2"},

    // Same rotation at the extremes of the legal B_w range. The twiddles are
    // the k=1,d=4 entry quantised to 10 and 24 bits; at this value magnitude
    // all three agree, which is the point -- B_w is precision, not meaning.
    {{1000, -2000}, {362, -362}, 10, {-707, -2121}, "B_w = 10 lower bound"},
    {{1000, -2000}, {5931642, -5931642}, 24, {-707, -2121}, "B_w = 24 upper bound"},

    // Intermediate stress. w here has magnitude sqrt(2), not 1, so it is not a
    // legal twiddle -- it is chosen because P_im = 67108863 * 65535 is about
    // 4.4e12 and wraps in int32. These two cases fail loudly if the
    // intermediates are ever narrowed.
    {{67108863, -67108863}, {-32768, 32767}, 16, {-2048, 134215678}, "int32 would wrap (max)"},
    {{-67108864, -67108864}, {32767, -32768}, 16, {-134215680, 2048}, "int32 would wrap (min)"},

    {{0, 0}, {23170, -23170}, 16, {0, 0}, "zero in, zero out"},
};

// The 3-multiplier (Karatsuba) form of the same product:
//   k1 = w_re*(v_re + v_im)
//   k2 = v_re*(w_im - w_re)
//   k3 = v_im*(w_re + w_im)
//   P_re = k1 - k3        P_im = k1 + k2
// Expanding both gives section 5.2's expressions exactly, so this must round
// to the same integers.
SdfComplex rotateKaratsuba(const SdfComplex& v, const SdfComplex& w, std::size_t B_w) {
  const std::int64_t v_re = v.re;
  const std::int64_t v_im = v.im;
  const std::int64_t w_re = w.re;
  const std::int64_t w_im = w.im;

  const std::int64_t k1 = w_re * (v_re + v_im);
  const std::int64_t k2 = v_re * (w_im - w_re);
  const std::int64_t k3 = v_im * (w_re + w_im);

  SdfComplex out{};
  out.re = static_cast<std::int32_t>(roundShift(k1 - k3, B_w));
  out.im = static_cast<std::int32_t>(roundShift(k1 + k2, B_w));
  return out;
}

// Deterministic, so a failure is reproducible without a seed to record.
std::uint64_t next(std::uint64_t& s) {
  s = s * 6364136223846793005ull + 1442695040888963407ull;
  return s >> 33;
}

bool runTable() {
  bool ok = true;
  for (const auto& c : kCases) {
    const auto got = rotate(c.v, c.w, c.B_w);
    if (got != c.expect) {
      std::printf("  FAIL %-28s v={%d,%d} w={%d,%d} B_w=%zu expected {%d,%d} got {%d,%d}\n", c.what,
                  c.v.re, c.v.im, c.w.re, c.w.im, c.B_w, c.expect.re, c.expect.im, got.re, got.im);
      ok = false;
    }
  }
  std::printf("  [%s] table, %zu cases\n", ok ? "PASS" : "FAIL",
              sizeof(kCases) / sizeof(kCases[0]));
  return ok;
}

bool runShiftIsArithmetic() {
  // Sign must propagate, and the shift must floor rather than truncate toward
  // zero: -1 >> 1 is -1, not 0.
  const bool ok = ((std::int64_t{-1} >> 1) == -1) && ((std::int64_t{-81920} >> 15) == -3) &&
                  (roundShift(std::int64_t{-81920}, 16) == -2);
  std::printf("  [%s] >> is an arithmetic shift\n", ok ? "PASS" : "FAIL");
  return ok;
}

bool runStructureEquivalence() {
  bool ok = true;
  std::uint64_t s = 0x5DF7CAFEull;
  std::size_t checked = 0;

  const std::size_t widths[] = {10, 16, 24};
  for (const auto B_w : widths) {
    // Stage values at the real target's width (27 bits signed), and twiddles
    // drawn from the actual table so the test exercises representable angles.
    for (std::size_t i = 0; i < 20000; ++i) {
      SdfComplex v{};
      v.re = static_cast<std::int32_t>(static_cast<std::int64_t>(next(s) % 134217728u) - 67108864);
      v.im = static_cast<std::int32_t>(static_cast<std::int64_t>(next(s) % 134217728u) - 67108864);

      const std::size_t d = std::size_t{1} << (1 + next(s) % 12); // 2 .. 4096
      const std::size_t k = 1 + next(s) % (d - 1);                // 1 .. d-1
      const auto w = twiddle(d, k, B_w);

      const auto a = rotate(v, w, B_w);
      const auto b = rotateKaratsuba(v, w, B_w);
      ++checked;
      if (a != b) {
        std::printf("  FAIL 3-mult vs 4-mult v={%d,%d} w={%d,%d} B_w=%zu: {%d,%d} vs {%d,%d}\n",
                    v.re, v.im, w.re, w.im, B_w, a.re, a.im, b.re, b.im);
        ok = false;
        break;
      }
    }
  }

  std::printf("  [%s] 3-mult and 4-mult agree, %zu samples\n", ok ? "PASS" : "FAIL", checked);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= runTable();
  ok &= runShiftIsArithmetic();
  ok &= runStructureEquivalence();

  std::printf("[SDF_MULTIPLY] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
