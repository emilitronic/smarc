// **********************************************************************
// smesh/tests/unit/accum_response/tb_normalizer_slots.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 3 2026

// Focused Reset-packet storage and handshake test for the two Normalizer slots.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "Normalizer.hpp"

#include <cstdio>

namespace {

smesh::AccNormReq packet(std::uint16_t slot, std::uint16_t cmd_id, smesh::Acc value) {
  smesh::AccNormReq req{};
  req.cmd.stats_id          = slot;
  req.cmd.cmd               = static_cast<std::uint8_t>(smesh::NormCmd::Reset);
  req.acc_read_resp.cmd_id  = cmd_id;
  req.acc_read_resp.data[0] = value;
  return req;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit,               req_val);
  Output(smesh::AccNormReq, req_bits);
  Input(bit,                req_rdy);
  Input(bit,                resp_val);
  Input(smesh::AccNormReq,  resp_bits);
  Output(bit,               resp_rdy);

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
  UPDATE(updateCheck).reads(phase_Q_, req_val, req_rdy, resp_val, resp_bits, resp_rdy)
                     .writes(phase_D_);
}

void Driver::updateDrive() {
  const auto phase = static_cast<std::uint8_t>(*phase_Q_);
  req_val = bit(phase <= 3 || phase == 6 || phase == 7);
  auto req = phase == 0 ? packet(0, 10, 100)
           : phase == 1 ? packet(1, 11, 110)
           : packet(0, 12, 120);
  if (phase == 6) {
    req.cmd.cmd = static_cast<std::uint8_t>(smesh::NormCmd::Variance);
  }
  if (phase == 7) req = packet(2, 13, 130);
  req_bits = req;
  resp_rdy = bit(phase >= 3);
}

void Driver::updateCheck() {
  const auto phase = static_cast<std::uint8_t>(*phase_Q_);
  if (Sim::state == Sim::SimResetting) return;

  const bool valid = resp_val == 1;
  const auto resp = *resp_bits;
  const auto cmd_id = static_cast<std::uint16_t>(resp.acc_read_resp.cmd_id);
  const auto value = resp.acc_read_resp.data[0];
  bool good = true;
  switch (phase) {
    case 0: good = req_rdy == 1 && !valid; break;
    case 1: good = req_rdy == 1 && valid && cmd_id == 10 && value == 100; break;
    case 2: good = req_rdy == 0 && valid && cmd_id == 10 && value == 100; break;
    case 3: good = req_rdy == 1 && valid && cmd_id == 10 && value == 100; break;
    case 4: good = valid && cmd_id == 12 && value == 120; break;
    case 5: good = valid && cmd_id == 11 && value == 110; break;
    case 6:
    case 7: good = req_rdy == 0 && !valid; break;
    case 8: good = !valid; done_ = true; break;
    default: break;
  }
  if (!good) {
    std::printf("[NORMALIZER_SLOTS] phase=%u req_rdy=%u resp_val=%u cmd_id=%u value=%d\n",
                static_cast<unsigned>(phase), static_cast<unsigned>(req_rdy == 1),
                static_cast<unsigned>(valid), static_cast<unsigned>(cmd_id),
                static_cast<int>(value));
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

  smesh::Normalizer normalizer("Normalizer");
  Driver driver("Driver");
  normalizer.req_val << driver.req_val;
  normalizer.req_bits << driver.req_bits;
  driver.req_rdy << normalizer.req_rdy;
  driver.resp_val << normalizer.resp_val;
  driver.resp_bits << normalizer.resp_bits;
  normalizer.resp_rdy << driver.resp_rdy;

  Clock clk;
  normalizer.clk << clk;
  driver.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 12 && !driver.done(); ++cycle) Sim::run();

  const bool ok = driver.done() && driver.passed();
  std::printf("[NORMALIZER_SLOTS] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
