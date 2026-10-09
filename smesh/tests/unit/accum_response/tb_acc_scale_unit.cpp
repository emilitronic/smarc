// **********************************************************************
// smesh/tests/unit/accum_response/tb_acc_scale_unit.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026
/*
Check ordinary scaling on the live accumulator response path, including stalls
and replacing a consumed response in the same cycle.
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "AccScaleUnit.hpp"

#include <cstdio>
#include <cstdint>

namespace {

smesh::AccScaleReq request(unsigned id) {
  smesh::AccScaleReq req{};
  auto& row = req.norm.acc_read_resp;
  row.laddr = smesh::makeAccAddr(id * smesh::kAccBankRows + id);
  row.from_dma = bit(id == 0);
  row.scale = id == 0 ? 0x3f000000u : 0x40000000u;
  row.data[0] = id == 0 ? 3 : 100;
  row.data[1] = id == 0 ? 5 : -65;
  row.data[2] = id == 0 ? -3 : 63;
  row.data[3] = id == 0 ? -5 : -64;
  return req;
}

bool matches(const smesh::AccScaleResp& actual, unsigned id) {
  const auto source = request(id).norm.acc_read_resp;
  const int first[] = {id == 0 ? 2 : 127, id == 0 ? 2 : -128,
                       id == 0 ? -2 : 126, id == 0 ? -2 : -128};
  if (actual.from_dma != source.from_dma || actual.acc_bank_id != source.laddr.acc_bank()) return false;
  for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
    const int expected = lane < 4 ? first[lane] : 0;
    if (actual.full_data[lane] != source.data[lane] || actual.data[lane] != expected) return false;
  }
  return true;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, req_val);
  Output(smesh::AccScaleReq, req_bits);
  Output(bit, out_rdy_issue);
  Output(bit, out_rdy_exresp);
  Input(bit, req_rdy);
  Input(bit, out_val);
  Input(smesh::AccScaleResp, out_bits);
  void updateDrive();
  void updateCheck();
  void reset() override;
  bool passed = true;
  unsigned accepted = 0;
  unsigned completed = 0;

 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  Output(u8, sent_Q_);
  Register(u8, sent_D_);
  Output(u8, seen_Q_);
  Register(u8, seen_D_);
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  sent_Q_ <= sent_D_;
  seen_Q_ <= seen_D_;
  UPDATE(updateDrive).reads(cycle_Q_, sent_Q_)
                     .writes(req_val, req_bits, out_rdy_issue, out_rdy_exresp);
  UPDATE(updateCheck).reads(cycle_Q_, sent_Q_, seen_Q_, req_val, req_rdy,
                            out_val, out_bits, out_rdy_issue)
                     .reads(out_rdy_exresp)
                     .writes(cycle_D_, sent_D_, seen_D_);
}

void Driver::updateDrive() {
  const auto cycle = static_cast<unsigned>(static_cast<std::uint8_t>(*cycle_Q_));
  const auto sent = static_cast<unsigned>(static_cast<std::uint8_t>(*sent_Q_));
  req_val = bit(sent < 2);
  req_bits = request(sent < 2 ? sent : 0);
  out_rdy_issue = bit(cycle >= 3);
  out_rdy_exresp = bit(cycle >= 5);
}

void Driver::updateCheck() {
  const auto cycle = static_cast<unsigned>(static_cast<std::uint8_t>(*cycle_Q_));
  const auto sent = static_cast<unsigned>(static_cast<std::uint8_t>(*sent_Q_));
  const auto seen = static_cast<unsigned>(static_cast<std::uint8_t>(*seen_Q_));
  const bool ready = seen == 0 ? out_rdy_issue == 1 : out_rdy_exresp == 1;

  if (cycle >= 1 && cycle <= 5) {
    passed &= out_val == 1 && seen < 2 && matches(*out_bits, seen);
  }
  if (cycle >= 6) passed &= out_val == 0;
  if (cycle >= 1 && cycle <= 2) passed &= req_rdy == 0;
  if (cycle == 3) passed &= req_rdy == 1;

  if (req_val == 1 && req_rdy == 1) {
    sent_D_ = u8(sent + 1);
    ++accepted;
  }
  if (out_val == 1 && ready) {
    seen_D_ = u8(seen + 1);
    ++completed;
  }
  cycle_D_ = u8(cycle + 1);
}

void Driver::reset() {
  cycle_Q_.reset(0);
  cycle_D_.reset(0);
  sent_Q_.reset(0);
  sent_D_.reset(0);
  seen_Q_.reset(0);
  seen_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::AccScaleReq{});
  out_rdy_issue.reset(0);
  out_rdy_exresp.reset(0);
  passed = true;
  accepted = 0;
  completed = 0;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  smesh::AccScaleUnit scale("Scale");
  Driver driver("Driver");
  Clock clk;
  scale.clk << clk;
  driver.clk << clk;
  scale.req_val << driver.req_val;
  scale.req_bits << driver.req_bits;
  scale.out_rdy_issue << driver.out_rdy_issue;
  scale.out_rdy_exresp << driver.out_rdy_exresp;
  driver.req_rdy << scale.req_rdy;
  driver.out_val << scale.out_val;
  driver.out_bits << scale.out_bits;
  clk.generateClock();
  Sim::init();
  Sim::reset();
  for (unsigned cycle = 0; cycle < 8; ++cycle) Sim::run();
  const bool good = driver.passed && driver.accepted == 2 && driver.completed == 2;
  std::printf("[ACC_SCALE_UNIT] width=%u accepted=%u completed=%u %s\n",
              static_cast<unsigned>(smesh::kDim), driver.accepted, driver.completed,
              good ? "PASS" : "FAIL");
  return good ? 0 : 1;
}
