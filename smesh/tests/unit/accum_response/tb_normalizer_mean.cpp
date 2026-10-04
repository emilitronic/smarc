// **********************************************************************
// smesh/tests/unit/accum_response/tb_normalizer_mean.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026
/*
Focused mean test.
*/
#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "Normalizer.hpp"

#include <array>
#include <cstdint>
#include <cstdio>

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
  req_val = bit(phase <= 2 || phase == 6);
  smesh::AccNormReq req{};
  switch (phase) {
    case 0: req = packet(0, smesh::NormCmd::Sum, 20, 2, {2, 2, 0, 0}); break;
    case 1: req = packet(1, smesh::NormCmd::Sum, 21, 2, {-5, -2, 0, 0}); break;
    case 2: req = packet(1, smesh::NormCmd::Mean, 22, 2, {-4, -3, 0, 0}); break;
    case 6: req = packet(1, smesh::NormCmd::Reset, 23, 0, {8, 7, 6, 5}); break;
    default: break;
  }
  req_bits = req;
  resp_rdy = bit(phase >= 8);
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto phase = static_cast<std::uint8_t>(*phase_Q_);
  const auto current = *stats;
  const auto response = *resp_bits;
  const auto sum0 = static_cast<std::uint32_t>(current.sum[0]);
  const auto sum1 = static_cast<std::int32_t>(static_cast<std::uint32_t>(current.sum[1]));
  const auto count1 = static_cast<std::uint16_t>(current.count[1]);
  bool good = true;

  switch (phase) {
    case 0: good = req_rdy == 1 && resp_val == 0 && current.mean[1] == 0; break;
    case 1: good = req_rdy == 1 && sum0 == 0 && count1 == 0; break;
    case 2: good = req_rdy == 1 && sum0 == 4 && sum1 == 0 && count1 == 2; break;
    case 3: good = sum1 == -7 && count1 == 4; break;
    case 4: good = sum1 == -14 && count1 == 4 && current.mean[1] == 0; break;
    case 5: good = sum1 == 0 && count1 == 0 && current.mean[1] == 0; break;
    case 6: good = req_rdy == 1 && current.mean[1] == -3 && sum0 == 4; break;
    case 7: good = resp_val == 1 && response.acc_read_resp.cmd_id == 23 &&
                   response.mean == -3 && current.mean[1] == -3; break;
    case 8: good = resp_val == 1 && response.mean == -3; break;
    case 9: good = resp_val == 0 && current.mean[1] == -3; done_ = true; break;
    default: break;
  }
  if (!good) {
    std::printf("[NORMALIZER_MEAN] phase=%u ready=%u out=%u sum={%u,%d} count1=%u mean1=%d resp_mean=%d\n",
                static_cast<unsigned>(phase), static_cast<unsigned>(req_rdy == 1),
                static_cast<unsigned>(resp_val == 1), sum0, sum1,
                count1, current.mean[1], response.mean);
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
  std::printf("[NORMALIZER_MEAN] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
