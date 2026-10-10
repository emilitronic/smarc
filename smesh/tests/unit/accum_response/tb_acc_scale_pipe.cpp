// **********************************************************************
// smesh/tests/unit/accum_response/tb_acc_scale_pipe.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 9 2026
/*
Feed identical elements to one-cycle and four-cycle pipes. Check exact return
cycles, consecutive results, bubbles, ties-to-even rounding, clipping, ReLU
before scaling, LayerNorm mean/scale selection and subtraction overflow,
IGELU polynomial coefficients and overflow,
disabling activation, unchanged full_data and destination fields,
and clearing in-flight elements on reset.
*/
// cmake --build build --target tb_acc_scale_pipe -j 4
// ./build/smesh/tb_acc_scale_pipe -trace '*'/acc_scale_pipe_view_

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "AccScalePipe.hpp"

#include <array>
#include <cstdio>
#include <limits>

TraceKey(acc_scale_pipe_view_);

namespace {

constexpr std::array<unsigned, 3> kLatencies{{1, 4, 4}};
constexpr std::array<bool, 3> kActivations{{true, true, false}};
constexpr std::array<int, 36> kInputs{{3, 5, -3, -5, 100, -65, 63, -64,
    -3, 5, -7, 7, std::numeric_limits<smesh::Acc>::min(),
    std::numeric_limits<smesh::Acc>::max(), 0, -5, 11, -1, 10, 1000, -1000,
    std::numeric_limits<smesh::Acc>::min(), std::numeric_limits<smesh::Acc>::max(), 15,
    -8, -4, -1, 1, 3, 8, std::numeric_limits<smesh::Acc>::min(),
    std::numeric_limits<smesh::Acc>::max(), 1, -1, 50000, -50000}};
constexpr std::array<std::uint32_t, 36> kScales{{
    0x3f000000u, 0x3f000000u, 0x3f000000u, 0x3f000000u,
    0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u,
    0x3f000000u, 0x3f000000u, 0xc0000000u, 0xc0000000u,
    0x3f800000u, 0x3f800000u, 0x3f000000u, 0x3f800000u,
    0x40000000u, 0x3f800000u, 0x40000000u, 0x3f800000u,
    0x3f800000u, 0x3f800000u, 0x3f800000u, 0x40400000u,
    0x3f800000u, 0x3f800000u, 0x3f000000u, 0x3f000000u,
    0x3f000000u, 0x3f000000u, 0x3f800000u, 0x3f800000u,
    0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u}};
constexpr std::array<int, 36> kOutputs{{2, 2, -2, -2, 127, -128, 126, -128,
    0, 2, 0, -14, 0, 127, 0, -5, 4, -2, 0, 127, -128, 127, -128, 2,
    0, 0, 4, 20, 50, 127, -128, -32, 2, 0, -128, -128}};
constexpr std::array<int, 36> kOutputsWithoutActivation{{2, 2, -2, -2, 127, -128, 126, -128,
    -2, 2, 14, -14, -128, 127, 0, -5, 22, -1, 20, 127, -128, -128, 127, 45,
    -8, -4, 0, 0, 2, 4, -128, 127, 1, -1, 127, -128}};
constexpr std::array<int, 8> kMeans{{4, 4, 10, -24, 24, 1, -1, 7}};
constexpr std::array<std::uint32_t, 8> kInvStddev{{0x3f000000u, 0x3f000000u,
    0x40000000u, 0x3f000000u, 0x3f000000u, 0x3f800000u, 0x3f800000u, 0x3e800000u}};
constexpr std::array<int, 12> kIgeluQb{{-4, -4, -4, -4, -4, -4, -3, -4, -65537, -65537, -100000, -100000}};
constexpr std::array<int, 12> kIgeluQc{{16, 16, 16, 16, 16, 16, 7, 16, 1, 1, 1, 1}};

// Thirty-six elements, with two empty input cycles after the first three.
int inputId(int cycle) {
  if (cycle >= 0 && cycle <= 2) return cycle;
  if (cycle >= 5 && cycle <= 37) return cycle - 2;
  return -1;
}

smesh::AccScaleElem element(unsigned id) {
  smesh::AccScaleElem value{};
  value.data = kInputs[id];
  value.full_data = 1000 + id; // distinct from data to detect accidental replacement
  value.scale = kScales[id];
  value.act = u8(id >= 24 ? 3 : id >= 16 ? 2 : id >= 8 && id < 15 ? 1 : 0);
  if (id >= 16 && id < 24) {
    value.mean = kMeans[id - 16];
    value.inv_stddev = kInvStddev[id - 16];
  }
  if (id >= 24) {
    value.igelu_qb = u32(static_cast<std::uint32_t>(kIgeluQb[id - 24]));
    value.igelu_qc = u32(static_cast<std::uint32_t>(kIgeluQc[id - 24]));
  }
  value.slot = u8(id % 3);
  value.element = u16(id % smesh::kDim);
  return value;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, in_val);
  Output(smesh::AccScaleElem, in_bits);
  InputArray(bit, out_val, 3);
  InputArray(smesh::AccScaleResult, out_bits, 3);
  void updateDrive();
  void updateCheck();
  void reset() override;
  bool passed = true;
  std::array<unsigned, 3> returns{};

 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_).writes(in_val, in_bits);
  UPDATE(updateCheck).reads(cycle_Q_, out_val, out_bits).writes(cycle_D_);
}

void Driver::updateDrive() {
  const int id = inputId(static_cast<std::uint8_t>(*cycle_Q_));
  in_val = bit(id >= 0);
  smesh::AccScaleElem value{};
  if (id >= 0) value = element(id);
  else value.act = 4; // invalid input must not trigger arithmetic or activation checks
  in_bits = value;
}

void Driver::updateCheck() {
  const int cycle = static_cast<std::uint8_t>(*cycle_Q_);
  for (unsigned pipe = 0; pipe < kLatencies.size(); ++pipe) {
    const int id = inputId(cycle - static_cast<int>(kLatencies[pipe]));
    bool good = (out_val[pipe] == 1) == (id >= 0);
    if (out_val[pipe] == 1) {
      ++returns[pipe];
      if (id >= 0) {
        const auto source = element(id);
        const auto result = *out_bits[pipe];
        const int expected = kActivations[pipe] ? kOutputs[id] : kOutputsWithoutActivation[id];
        good = good && result.data == expected && result.full_data == source.full_data &&
               result.slot == source.slot && result.element == source.element;
        trace(acc_scale_pipe_view_, "cycle=%02d latency=%u activation=%u slot=%u element=%u scaled=%d\n",
              cycle, kLatencies[pipe], kActivations[pipe], static_cast<unsigned>(result.slot),
              static_cast<unsigned>(result.element), static_cast<int>(result.data));
      }
    }
    if (!good) std::printf("[ACC_SCALE_PIPE] mismatch cycle=%d latency=%u activation=%u\n",
                           cycle, kLatencies[pipe], kActivations[pipe]);
    passed = passed && good;
  }
  cycle_D_ = u8(cycle + 1);
}

void Driver::reset() {
  cycle_Q_.reset(0);
  cycle_D_.reset(0);
  in_val.reset(0);
  in_bits.reset(smesh::AccScaleElem{});
  passed = true;
  returns = {};
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  smesh::AccScalePipe short_pipe("OneCycle", kLatencies[0]);
  smesh::AccScalePipe long_pipe("FourCycle", kLatencies[1]);
  smesh::AccScalePipe disabled_pipe("NoActivations", kLatencies[2], false);
  Driver driver("Driver");
  Clock clk;
  short_pipe.clk << clk;
  long_pipe.clk << clk;
  disabled_pipe.clk << clk;
  driver.clk << clk;
  short_pipe.in_val << driver.in_val;
  short_pipe.in_bits << driver.in_bits;
  long_pipe.in_val << driver.in_val;
  long_pipe.in_bits << driver.in_bits;
  disabled_pipe.in_val << driver.in_val;
  disabled_pipe.in_bits << driver.in_bits;
  driver.out_val[0] << short_pipe.out_val;
  driver.out_bits[0] << short_pipe.out_bits;
  driver.out_val[1] << long_pipe.out_val;
  driver.out_bits[1] << long_pipe.out_bits;
  driver.out_val[2] << disabled_pipe.out_val;
  driver.out_bits[2] << disabled_pipe.out_bits;
  clk.generateClock();
  Sim::init();
  Sim::reset();
  for (unsigned cycle = 0; cycle < 7; ++cycle) Sim::run();
  bool good = driver.passed;
  Sim::reset(); // discard elements still in the four-cycle pipeline
  good = good && short_pipe.out_val == 0 && long_pipe.out_val == 0 && disabled_pipe.out_val == 0;
  for (unsigned cycle = 0; cycle < 44; ++cycle) Sim::run();
  good = good && driver.passed && driver.returns[0] == 36 && driver.returns[1] == 36 && driver.returns[2] == 36;
  std::printf("[ACC_SCALE_PIPE] latency=1 returns=%u latency=4 returns=%u disabled returns=%u %s\n",
              driver.returns[0], driver.returns[1], driver.returns[2], good ? "PASS" : "FAIL");
  descore::flushLog();
  return good ? 0 : 1;
}
