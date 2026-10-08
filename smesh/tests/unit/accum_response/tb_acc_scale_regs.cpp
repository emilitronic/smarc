// **********************************************************************
// smesh/tests/unit/accum_response/tb_acc_scale_regs.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026
/*
Fill Original's three input regs slots, with an idle cycle between packets.
Check next-cycle visibility, complete packet retention, full-bank backpressure,
matching out_regs source/bank metadata, and reset while all slots are occupied.
Also compiled for DIM=8.
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "AccScaleRegs.hpp"

#include <cstdio>

namespace {

smesh::AccScaleReq packet(unsigned id) {
  smesh::AccScaleReq req{};
  auto& row = req.norm.acc_read_resp;
  for (std::size_t element = 0; element < smesh::AccScaleRegs::kWidth; ++element) {
    row.data[element] = -static_cast<smesh::Acc>(100 * (id + 1) + element);
  }
  row.laddr = smesh::makeAccAddr((id % smesh::kAccBanks) * smesh::kAccBankRows + id);
  row.mask = 5 + id;
  row.len = smesh::AccScaleRegs::kWidth;
  row.act = 1 + id;
  row.scale = 0x3f800000u + id;
  row.igelu_qb = 10 + id;
  row.igelu_qc = 20 + id;
  row.iexp_qln2 = 30 + id;
  row.iexp_qln2_inv = 40 + id;
  row.full = bit(id % 2);
  row.from_dma = bit((id + 1) % 2);
  row.cmd_id = 100 + id;
  req.norm.cmd = {u16(2 + id), u16(id % 2), u8(1 + id)};
  req.norm.mean = -10 - static_cast<smesh::Acc>(id);
  req.norm.max = id == 1 ? 27 : -20 - static_cast<smesh::Acc>(id);
  req.norm.inv_stddev = 0x3f000000u + id;
  req.norm.inv_sum_exp = 0x3e800000u + id;
  return req;
}

bool samePacket(const smesh::AccScaleReq& actual, const smesh::AccScaleReq& expected) {
  const auto& a = actual.norm.acc_read_resp;
  const auto& e = expected.norm.acc_read_resp;
  return a.data == e.data && a.laddr.raw == e.laddr.raw && a.mask == e.mask &&
         a.len == e.len && a.act == e.act && a.scale == e.scale &&
         a.igelu_qb == e.igelu_qb && a.igelu_qc == e.igelu_qc &&
         a.iexp_qln2 == e.iexp_qln2 && a.iexp_qln2_inv == e.iexp_qln2_inv &&
         a.full == e.full && a.from_dma == e.from_dma && a.cmd_id == e.cmd_id &&
         actual.norm.cmd.len == expected.norm.cmd.len &&
         actual.norm.cmd.stats_id == expected.norm.cmd.stats_id &&
         actual.norm.cmd.cmd == expected.norm.cmd.cmd && actual.norm.mean == expected.norm.mean &&
         actual.norm.max == expected.norm.max &&
         actual.norm.inv_stddev == expected.norm.inv_stddev &&
         actual.norm.inv_sum_exp == expected.norm.inv_sum_exp;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, req_val);
  Output(smesh::AccScaleReq, req_bits);
  Input(bit, req_rdy);
  InputArray(bit, regs_val, smesh::AccScaleRegs::kEntries);
  InputArray(smesh::AccScaleReq, regs_bits, smesh::AccScaleRegs::kEntries);
  InputArray(smesh::AccScaleResp, out_regs, smesh::AccScaleRegs::kEntries);
  void update();
  void reset() override;
  bool passed = true;
 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  UPDATE(update).reads(cycle_Q_, req_rdy, regs_val, regs_bits, out_regs)
                .writes(req_val, req_bits, cycle_D_);
}

void Driver::update() {
  const auto cycle = static_cast<std::uint8_t>(*cycle_Q_);
  // Packets enter at cycles 1, 2 and 4. A fourth waits at cycles 5 and 6.
  req_val = bit(cycle == 1 || cycle == 2 || cycle == 4 || cycle == 5 || cycle == 6);
  req_bits = packet(cycle == 1 ? 0 : cycle == 2 ? 1 : cycle == 4 ? 2 : 3);
  if (Sim::state == Sim::SimResetting) return;

  const unsigned occupied = cycle < 2 ? 0 : cycle == 2 ? 1 : cycle < 5 ? 2 : 3;
  bool good = (req_rdy == 1) == (occupied < smesh::AccScaleRegs::kEntries);
  for (std::size_t slot = 0; slot < smesh::AccScaleRegs::kEntries; ++slot) {
    good = good && (regs_val[slot] == 1) == (slot < occupied);
    good = good && samePacket(*regs_bits[slot], slot < occupied ? packet(slot) : smesh::AccScaleReq{});
    smesh::AccScaleResp expected_output{};
    if (slot < occupied) {
      const auto input = packet(slot).norm.acc_read_resp;
      expected_output.from_dma = input.from_dma;
      expected_output.acc_bank_id = u16(input.laddr.acc_bank());
    }
    good = good && out_regs[slot]->from_dma == expected_output.from_dma &&
           out_regs[slot]->acc_bank_id == expected_output.acc_bank_id &&
           out_regs[slot]->full_data == expected_output.full_data &&
           out_regs[slot]->data == expected_output.data;
  }
  if (!good) {
    std::printf("[ACC_SCALE_REGS] mismatch cycle=%u occupied=%u ready=%u\n",
                cycle, occupied, static_cast<unsigned>(req_rdy));
    passed = false;
  }
  cycle_D_ = u8(cycle + 1);
}

void Driver::reset() {
  cycle_Q_.reset(0);
  cycle_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::AccScaleReq{});
  passed = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  smesh::AccScaleRegs regs("Regs");
  Driver driver("Driver");
  regs.req_val << driver.req_val;
  regs.req_bits << driver.req_bits;
  driver.req_rdy << regs.req_rdy;
  for (std::size_t slot = 0; slot < smesh::AccScaleRegs::kEntries; ++slot) {
    driver.regs_val[slot] << regs.regs_val_Q_[slot];
    driver.regs_bits[slot] << regs.regs_bits_Q_[slot];
    driver.out_regs[slot] << regs.out_regs_Q_[slot];
  }
  Clock clk;
  regs.clk << clk;
  driver.clk << clk;
  clk.generateClock();
  Sim::init();
  bool good = true;
  for (int run = 0; run < 2; ++run) {
    Sim::reset();
    for (int cycle = 0; cycle < 8; ++cycle) Sim::run();
    good = good && driver.passed;
  }
  std::printf("[ACC_SCALE_REGS] slots=%u width=%u %s\n",
              static_cast<unsigned>(smesh::AccScaleRegs::kEntries),
              static_cast<unsigned>(smesh::AccScaleRegs::kWidth), good ? "PASS" : "FAIL");
  descore::flushLog();
  return good ? 0 : 1;
}
