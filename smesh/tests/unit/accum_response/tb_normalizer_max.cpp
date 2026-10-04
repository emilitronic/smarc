// **********************************************************************
// smesh/tests/unit/accum_response/tb_normalizer_max.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "Normalizer.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace {

smesh::AccNormReq packet(std::uint16_t slot, smesh::NormCmd cmd,
                         std::uint16_t id, std::uint16_t len,
                         std::array<smesh::Acc, smesh::kDim> data) {
  smesh::AccNormReq req{};
  req.cmd.stats_id = slot;
  req.cmd.cmd = static_cast<std::uint8_t>(cmd);
  req.cmd.len = len;
  req.acc_read_resp.cmd_id = id;
  req.acc_read_resp.data = data;
  return req;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, req_val);
  Output(smesh::AccNormReq, req_bits);
  Input(bit, req_rdy);
  Output(bit, resp_rdy);
  Input(bit, resp_val);
  Input(smesh::AccNormReq, resp_bits);
  Input(smesh::NormStatsRegs, stats);

  void updateDrive();
  void updateCheck();
  void reset() override;

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  Output(u8, phase_Q_);
  Register(u8, phase_D_);
  bool done_ = false;
  bool passed_ = true;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  phase_Q_ <= phase_D_;
  UPDATE(updateDrive).reads(phase_Q_).writes(req_val, req_bits, resp_rdy);
  UPDATE(updateCheck).reads(phase_Q_, req_val, req_rdy, resp_val, resp_bits, stats)
                     .writes(phase_D_);
}

void Driver::updateDrive() {
  const auto phase = static_cast<std::uint8_t>(*phase_Q_);
  req_val = bit(phase == 0 || phase == 1 || phase == 4 || phase == 5 || phase == 6);
  smesh::AccNormReq req{};
  switch (phase) {
    case 0: req = packet(1, smesh::NormCmd::Sum, 20, 4, {1, 2, 3, 4}); break;
    case 1: req = packet(0, smesh::NormCmd::Max, 21, 3, {-9, -4, -7, 99}); break;
    case 4: req = packet(0, smesh::NormCmd::Max, 22, 2, {-8, -6, 99, 99}); break;
    case 5:
    case 6: req = packet(0, smesh::NormCmd::Reset, 23, 0, {3, 4, 5, 6}); break;
    default: break;
  }
  req_bits = req;
  resp_rdy = bit(phase >= 8);
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto phase = static_cast<std::uint8_t>(*phase_Q_);
  const auto current = *stats;
  const auto max0 = current.max[0];
  const auto running0 = current.running_max[0];
  const auto left0 = static_cast<std::uint16_t>(current.elems_left[0]);
  const auto sum1 = static_cast<std::uint32_t>(current.sum[1]);
  constexpr auto min = std::numeric_limits<smesh::Acc>::min();
  bool good = true;

  switch (phase) {
    case 0: good = req_rdy == 1 && max0 == min && running0 == min; break;
    case 1: good = req_rdy == 1 && sum1 == 0 && max0 == min; break;
    case 2: good = left0 == 3 && sum1 == 7 && max0 == min; break;
    case 3: good = left0 == 2 && sum1 == 10 && max0 == -7; break;
    case 4: good = req_rdy == 1 && left0 == 0 && max0 == -4 && running0 == -4; break;
    case 5: good = req_rdy == 0 && left0 == 2 && max0 == -4; break;
    case 6: good = req_rdy == 1 && left0 == 0 && max0 == -4; break;
    case 7: good = resp_val == 1 && resp_bits->acc_read_resp.cmd_id == 23 &&
                   running0 == -4 && max0 == -4; break;
    case 8: good = resp_val == 1 && running0 == min && max0 == -4; break;
    case 9: good = resp_val == 0 && max0 == -4; done_ = true; break;
    default: break;
  }
  if (!good) {
    std::printf("[NORMALIZER_MAX] phase=%u ready=%u out=%u left0=%u sum1=%u max0=%d running0=%d\n",
                static_cast<unsigned>(phase), static_cast<unsigned>(req_rdy == 1),
                static_cast<unsigned>(resp_val == 1), left0, sum1, max0, running0);
    passed_ = false;
  }
  if (!done_) phase_D_ = u8(phase + 1);
}

void Driver::reset() {
  phase_Q_.reset(0);
  phase_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::AccNormReq{});
  resp_rdy.reset(0);
  done_ = false;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::Normalizer normalizer("Normalizer", 2);
  Driver driver("Driver");
  normalizer.req_val << driver.req_val;
  normalizer.req_bits << driver.req_bits;
  driver.req_rdy << normalizer.req_rdy;
  normalizer.resp_rdy << driver.resp_rdy;
  driver.resp_val << normalizer.resp_val;
  driver.resp_bits << normalizer.resp_bits;
  driver.stats << normalizer.stats_view;

  Clock clk;
  normalizer.clk << clk;
  driver.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 14 && !driver.done(); ++cycle) Sim::run();

  const bool ok = driver.done() && driver.passed();
  std::printf("[NORMALIZER_MAX] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
