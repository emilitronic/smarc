// **********************************************************************
// smesh/tests/unit/accum_response/tb_normalizer_inv_stddev.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "Normalizer.hpp"

#include <cstdint>
#include <cstdio>

namespace {

smesh::AccNormReq packet(unsigned index) {
  smesh::AccNormReq req{};
  req.cmd.stats_id = 1;
  req.acc_read_resp.cmd_id = u16(30 + index);
  if (index == 0 || index == 3) {
    req.cmd.cmd = u8(static_cast<std::uint8_t>(smesh::NormCmd::Mean));
    req.cmd.len = 2;
    req.acc_read_resp.data[0] = index == 0 ? 2 : 3;
    req.acc_read_resp.data[1] = index == 0 ? 4 : 3;
  } else if (index == 1 || index == 4) {
    req.cmd.cmd = u8(static_cast<std::uint8_t>(smesh::NormCmd::InvStddev));
    req.cmd.len = 2;
    req.acc_read_resp.data[0] = index == 1 ? 1 : 3;
    req.acc_read_resp.data[1] = index == 1 ? 5 : 3;
    req.acc_read_resp.scale = index == 1 ? 0x3fc00000u : 0x3f800000u;
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
  unsigned responses_ = 0;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  next_Q_ <= next_D_;
  UPDATE(updateDrive).reads(next_Q_).writes(req_val, req_bits, resp_rdy);
  UPDATE(updateCheck).reads(next_Q_, req_val, req_rdy, resp_val, resp_bits, stats)
                     .writes(next_D_);
}

void Driver::updateDrive() {
  const auto next = static_cast<unsigned>(static_cast<std::uint8_t>(*next_Q_));
  req_val = bit(next < 6);
  req_bits = next < 6 ? packet(next) : smesh::AccNormReq{};
  resp_rdy = 1;
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto next = static_cast<unsigned>(static_cast<std::uint8_t>(*next_Q_));
  const auto current = *stats;
  if (req_val == 1 && req_rdy == 1) next_D_ = u8(next + 1);

  if (resp_val == 1) {
    const auto response = *resp_bits;
    const bool first = responses_ == 0;
    const bool good = next == (first ? 3u : 6u) &&
                      response.acc_read_resp.cmd_id == (first ? 32 : 35) &&
                      response.mean == 3 &&
                      static_cast<std::uint32_t>(response.inv_stddev) ==
                          (first ? 0x3f400000u : 0x3f800000u) &&
                      current.variance[1] == (first ? 4 : 0) &&
                      current.stddev[1] == (first ? 2 : 1) &&
                      current.mean[0] == 0 && current.inv_stddev[0] == 0;
    if (!good) {
      std::printf("[NORMALIZER_INV_STDDEV] next=%u id=%u mean=%d variance=%d stddev=%d inv=0x%x\n",
                  next, static_cast<unsigned>(response.acc_read_resp.cmd_id), response.mean,
                  current.variance[1], current.stddev[1],
                  static_cast<unsigned>(response.inv_stddev));
      passed_ = false;
    }
    ++responses_;
    done_ = responses_ == 2;
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
  responses_ = 0;
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
  for (int cycle = 0; cycle < 80 && !driver.done(); ++cycle) Sim::run();

  const bool ok = driver.done() && driver.passed();
  std::printf("[NORMALIZER_INV_STDDEV] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
