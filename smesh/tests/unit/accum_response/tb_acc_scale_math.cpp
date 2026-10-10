// **********************************************************************
// smesh/tests/unit/accum_response/tb_acc_scale_math.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026

#include "AccScaleMath.hpp"

#include <array>
#include <cstdio>
#include <cstdint>
#include <limits>

namespace {

constexpr std::uint32_t kScaleOne  = 0x3f800000u;
constexpr std::uint32_t kScaleHalf = 0x3f000000u;
constexpr std::uint32_t kScaleTwo  = 0x40000000u;

bool check(smesh::Acc input, std::uint32_t scale, smesh::Acc expected,
           smesh::Acc full_data = 123456) {
  smesh::AccScaleElem elem{};
  elem.data = input;
  elem.full_data = full_data;
  elem.scale = scale;
  elem.slot = 2;
  elem.element = 5;

  const auto result = smesh::scaleAccumulatorElement(elem);
  return result.data == expected && result.full_data == full_data &&
         result.slot == 2 && result.element == 5;
}

} // namespace

int main() {
  bool good = true;
  good &= check(42, kScaleOne, 42);
  good &= check(3, kScaleHalf, 2);
  good &= check(5, kScaleHalf, 2);
  good &= check(-3, kScaleHalf, -2);
  good &= check(-5, kScaleHalf, -2);
  good &= check(1, kScaleHalf, 0);
  good &= check(-1, kScaleHalf, 0);
  good &= check(127, 0, 0);
  good &= check(-3, 0xbf000000u, 2);
  good &= check(100, kScaleTwo, 127);
  good &= check(-65, kScaleTwo, -128);
  good &= check(std::numeric_limits<smesh::Acc>::max(), kScaleOne, 127);
  good &= check(std::numeric_limits<smesh::Acc>::min(), kScaleOne, -128);

  // Fixed integer results, before scale/clip can hide a polynomial overflow mistake.
  struct IgeluCase { smesh::Acc q, qb, qc, expected; };
  const std::array<IgeluCase, 18> igelu_cases{{
      {-8, -4, 16, 0}, {-4, -4, 16, 0}, {-3, -4, 16, 3},
      {-2, -4, 16, 8}, {-1, -4, 16, 9}, {0, -4, 16, 0},
      {1, -4, 16, 41}, {2, -4, 16, 72}, {3, -4, 16, 99},
      {4, -4, 16, 128}, {8, -4, 16, 256},
      {std::numeric_limits<smesh::Acc>::min(), -3, 7, std::numeric_limits<smesh::Acc>::min()},
      {std::numeric_limits<smesh::Acc>::max(), -4, 16, -32},
      {1, -65537, 1, 2}, {-1, -65537, 1, 0},
      {50000, -100000, 1, -728082784}, {-50000, -100000, 1, -728182784},
      {1, std::numeric_limits<smesh::Acc>::min(), std::numeric_limits<smesh::Acc>::max(), -2}
  }};
  for (const auto& item : igelu_cases) {
    const auto actual = smesh::igeluAccumValue(item.q, u32(static_cast<std::uint32_t>(item.qb)),
                                              u32(static_cast<std::uint32_t>(item.qc)));
    if (actual != item.expected) {
      std::printf("[ACC_SCALE_MATH] IGELU q=%d qb=%d qc=%d expected=%d actual=%d\n",
                  item.q, item.qb, item.qc, item.expected, actual);
      good = false;
    }
  }

  std::printf("[ACC_SCALE_MATH] %s\n", good ? "PASS" : "FAIL");
  return good ? 0 : 1;
}
