// **********************************************************************
// smesh/tests/unit/accum_response/tb_normalizer_sum_exp.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 5 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "Normalizer.hpp"

#include <cstdint>
#include <cstdio>

namespace {

smesh::AccNormReq packet(unsigned index) {
  smesh::AccNormReq req{};
  if (index == 0) {
    req.cmd.stats_id = 0;
    req.cmd.cmd = u8(static_cast<std::uint8_t>(smesh::NormCmd::Sum));
    req.cmd.len = 2;
    req.acc_read_resp.data[0] = 2;
    req.acc_read_resp.data[1] = 5;
  } else {
    req.cmd.stats_id = 1;
    req.acc_read_resp.data[0] = 5;
    req.acc_read_resp.data[1] = 3;
    req.acc_read_resp.data[2] = 4;
    if (index == 1) {
      req.cmd.cmd = u8(static_cast<std::uint8_t>(smesh::NormCmd::Max));
      req.cmd.len = 3;
    } else if (index == 2) {
      req.cmd.cmd = u8(static_cast<std::uint8_t>(smesh::NormCmd::SumExp));
      req.cmd.len = 3;
      req.acc_read_resp.igelu_qb = u32(0xfffffffdu); // -3
      req.acc_read_resp.igelu_qc = u32(2);
    } else {
      req.cmd.cmd = u8(static_cast<std::uint8_t>(smesh::NormCmd::Reset));
    }
  }
  req.acc_read_resp.cmd_id = u16(40 + index);
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
  Output(u8, next_Q_);
  Register(u8, next_D_);
  bool done_ = false;
  bool passed_ = true;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  next_Q_ <= next_D_;
  UPDATE(updateDrive).reads(next_Q_).writes(req_val, req_bits, resp_rdy);
  UPDATE(updateCheck).reads(next_Q_, req_val, req_rdy, resp_val, resp_bits, stats)
                     .writes(next_D_);
}

void Driver::updateDrive() {
  const auto next = static_cast<unsigned>(static_cast<std::uint8_t>(*next_Q_));
  req_val = bit(next < 4);
  req_bits = next < 4 ? packet(next) : smesh::AccNormReq{};
  resp_rdy = 1;
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto next = static_cast<unsigned>(static_cast<std::uint8_t>(*next_Q_));
  if (req_val == 1 && req_rdy == 1) next_D_ = u8(next + 1);

  if (resp_val == 1) {
    const auto values = *stats;
    const bool good = next == 4 && resp_bits->acc_read_resp.cmd_id == 43 &&
                      values.max[1] == 5 && values.sum[1] == 6 &&
                      values.sum[0] == 7;
    if (!good) {
      std::printf("[NORMALIZER_SUM_EXP] next=%u id=%u max1=%d sum1=%u sum0=%u\n",
                  next, static_cast<unsigned>(resp_bits->acc_read_resp.cmd_id),
                  values.max[1], static_cast<unsigned>(values.sum[1]),
                  static_cast<unsigned>(values.sum[0]));
      passed_ = false;
    }
    done_ = true;
  }
}

void Driver::reset() {
  next_Q_.reset(0);
  next_D_.reset(0);
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
  for (int cycle = 0; cycle < 40 && !driver.done(); ++cycle) Sim::run();

  const bool ok = driver.done() && driver.passed();
  std::printf("[NORMALIZER_SUM_EXP] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
