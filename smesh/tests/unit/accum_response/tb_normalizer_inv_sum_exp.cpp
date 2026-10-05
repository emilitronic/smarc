// **********************************************************************
// smesh/tests/unit/accum_response/tb_normalizer_inv_sum_exp.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 5 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "Normalizer.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

smesh::AccNormReq packet(unsigned index) {
  smesh::AccNormReq req{};
  req.cmd.stats_id = 1;
  req.acc_read_resp.cmd_id = u16(50 + index);
  req.acc_read_resp.data[0] = 5;
  req.acc_read_resp.data[1] = 3;
  req.acc_read_resp.data[2] = 4;
  if (index == 0) {
    req.cmd.cmd = u8(static_cast<std::uint8_t>(smesh::NormCmd::Max));
    req.cmd.len = 3;
  } else if (index == 1) {
    req.cmd.cmd = u8(static_cast<std::uint8_t>(smesh::NormCmd::InvSumExp));
    req.cmd.len = 3;
    req.acc_read_resp.igelu_qb = u32(0xfffffffdu); // -3
    req.acc_read_resp.igelu_qc = u32(2);
    const float scale = 2.0f;
    std::uint32_t bits = 0;
    std::memcpy(&bits, &scale, sizeof(bits));
    req.acc_read_resp.scale = u32(bits);
  } else {
    req.cmd.cmd = u8(static_cast<std::uint8_t>(smesh::NormCmd::Reset));
  }
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
  req_val = bit(next < 3);
  req_bits = next < 3 ? packet(next) : smesh::AccNormReq{};
  resp_rdy = 1;
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto next = static_cast<unsigned>(static_cast<std::uint8_t>(*next_Q_));
  if (req_val == 1 && req_rdy == 1) next_D_ = u8(next + 1);

  if (resp_val == 1) {
    const float expected = (127.0f / 6.0f) * 2.0f;
    std::uint32_t bits = 0;
    std::memcpy(&bits, &expected, sizeof(bits));
    const auto values = *stats;
    const bool good = next == 3 && resp_bits->acc_read_resp.cmd_id == 52 &&
                      values.max[1] == 5 && values.sum[1] == 6 &&
                      values.inv_sum_exp[1] == bits && resp_bits->inv_sum_exp == bits;
    if (!good) {
      std::printf("[NORMALIZER_INV_SUM_EXP] next=%u id=%u sum=%u inv=0x%x resp=0x%x\n",
                  next, static_cast<unsigned>(resp_bits->acc_read_resp.cmd_id),
                  static_cast<unsigned>(values.sum[1]),
                  static_cast<unsigned>(values.inv_sum_exp[1]),
                  static_cast<unsigned>(resp_bits->inv_sum_exp));
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
  for (int cycle = 0; cycle < 50 && !driver.done(); ++cycle) Sim::run();

  const bool ok = driver.done() && driver.passed();
  std::printf("[NORMALIZER_INV_SUM_EXP] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
