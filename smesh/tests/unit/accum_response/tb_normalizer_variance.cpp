// **********************************************************************
// smesh/tests/unit/accum_response/tb_normalizer_sum.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "Normalizer.hpp"

#include <array>
#include <cstdint>
#include <cstdio>

namespace {

smesh::AccNormReq packet(smesh::NormCmd cmd, std::uint16_t id,
                         std::uint16_t len, std::array<smesh::Acc, smesh::kDim> data) {
  smesh::AccNormReq req{};
  req.cmd.stats_id = 1;
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
  UPDATE(updateCheck).reads(phase_Q_, req_rdy, resp_val, resp_bits, stats)
                     .writes(phase_D_);
}

void Driver::updateDrive() {
  const auto phase = static_cast<std::uint8_t>(*phase_Q_);
  req_val = bit(phase == 0 || phase == 5 || phase == 8);
  smesh::AccNormReq req{};
  switch (phase) {
    case 0: req = packet(smesh::NormCmd::Mean, 20, 3, {1, 3, 5, 99}); break;
    case 5: req = packet(smesh::NormCmd::Variance, 21, 3, {-1, 3, 7, 99}); break;
    case 8: req = packet(smesh::NormCmd::Reset, 22, 0, {8, 7, 6, 5}); break;
    default: break;
  }
  req_bits = req;
  resp_rdy = bit(phase >= 10);
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto phase = static_cast<std::uint8_t>(*phase_Q_);
  const auto current = *stats;
  const auto response = *resp_bits;
  const auto sum = static_cast<std::uint32_t>(current.sum[1]);
  const auto count = static_cast<std::uint16_t>(current.count[1]);
  const auto left = static_cast<std::uint16_t>(current.elems_left[1]);
  bool good = true;

  switch (phase) {
    case 0: good = req_rdy == 1 && current.mean[1] == 0; break;
    case 1: good = sum == 0 && count == 3 && left == 3; break;
    case 2: good = sum == 5 && left == 2; break;
    case 3: good = sum == 9 && left == 0; break;
    case 4: good = sum == 0 && count == 0 && current.mean[1] == 0; break;
    case 5: good = req_rdy == 1 && current.mean[1] == 3; break;
    case 6: good = sum == 0 && count == 3 && left == 3; break;
    case 7: good = sum == 16 && left == 2; break;
    case 8: good = req_rdy == 1 && sum == 32 && count == 3 && left == 0; break;
    case 9: good = resp_val == 1 && response.acc_read_resp.cmd_id == 22 &&
                   response.mean == 3 && sum == 32; break;
    case 10: good = resp_val == 1 && response.mean == 3 && sum == 0; break;
    case 11: good = resp_val == 0 && current.mean[1] == 3; done_ = true; break;
    default: break;
  }
  if (!good) {
    std::printf("[NORMALIZER_VARIANCE] phase=%u ready=%u out=%u sum=%u count=%u left=%u mean=%d\n",
                static_cast<unsigned>(phase), static_cast<unsigned>(req_rdy == 1),
                static_cast<unsigned>(resp_val == 1), sum, count, left, current.mean[1]);
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
  for (int cycle = 0; cycle < 16 && !driver.done(); ++cycle) Sim::run();

  const bool ok = driver.done() && driver.passed();
  std::printf("[NORMALIZER_VARIANCE] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
