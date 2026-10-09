// **********************************************************************
// smesh/src/accum_response/AccScaleMath.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026

#include "AccScaleMath.hpp"

#include <cmath>
#include <cfenv>
#include <cstring>
#include <limits>

namespace smesh {

namespace {

float scaleFromBits(u32 bits) {
  static_assert(sizeof(float) == sizeof(bits), "Scale format must be binary32");
  static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24, "Accumulator scaling requires IEEE binary32");
  float scale = 0.0f;
  const auto raw = static_cast<std::uint32_t>(bits);
  std::memcpy(&scale, &raw, sizeof(scale));
  assert_always(std::isfinite(scale), "Nonfinite accumulator scale is not modeled");
  return scale;
}

std::int64_t roundTiesToEven(float value) {
  const double lower = std::floor(static_cast<double>(value));
  const auto integer = static_cast<std::int64_t>(lower);
  const double fraction = static_cast<double>(value) - lower;
  if (fraction > 0.5 || (fraction == 0.5 && integer % 2 != 0)) return integer + 1;
  return integer;
}

Acc convertToAcc(float value) {
  constexpr float kAccPositiveLimit = 2147483648.0f;
  constexpr float kAccNegativeLimit = -2147483648.0f;
  if (!std::isfinite(value) || value >= kAccPositiveLimit || value < kAccNegativeLimit) {
    return std::signbit(value) ? std::numeric_limits<Acc>::min() : std::numeric_limits<Acc>::max();
  }

  const auto rounded = roundTiesToEven(value);
  if (rounded > std::numeric_limits<Acc>::max()) return std::numeric_limits<Acc>::max();
  if (rounded < std::numeric_limits<Acc>::min()) return std::numeric_limits<Acc>::min();
  return static_cast<Acc>(rounded);
}

Elem clipToElem(Acc value) {
  if (value > std::numeric_limits<Elem>::max()) return std::numeric_limits<Elem>::max();
  if (value < std::numeric_limits<Elem>::min()) return std::numeric_limits<Elem>::min();
  return static_cast<Elem>(value);
}

} // namespace

Elem scaleAccumValue(Acc value, u32 scale_bits) {
  assert_always(std::fegetround() == FE_TONEAREST, "Accumulator scaling requires round-to-nearest host arithmetic");
  const float product = static_cast<float>(value) * scaleFromBits(scale_bits);
  const Acc scaled    = convertToAcc(product);
  return clipToElem(scaled);
}

AccScaleResult scaleAccumulatorElement(const AccScaleElem& input) {
  AccScaleResult result{};
  result.full_data = input.full_data;
  result.data      = scaleAccumValue(input.data, input.scale);
  result.slot      = input.slot;
  result.element   = input.element;
  return result;
}

} // namespace smesh
