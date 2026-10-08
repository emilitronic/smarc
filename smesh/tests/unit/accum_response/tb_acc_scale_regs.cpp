// **********************************************************************
// smesh/tests/unit/accum_response/tb_acc_scale_regs.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026
/*
Test Original's ordered slot control with the input/output register banks.
Supply completion masks to check output ordering, partial completion, stalls,
slot release, simultaneous release/store, pointer wrap and reset while full.
Also check complete packet retention and output metadata for DIM=4 and DIM=8.
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "AccScaleRegs.hpp"
#include "AccScaleSlotCtrl.hpp"

#include <cstdio>

TraceKey(acc_scale_slot_view_);

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
  Output(bit, out_rdy);
  OutputArray(smesh::AccScaleSlotCtrl::CompletedMask, completed_masks, smesh::AccScaleRegs::kEntries);
  Input(bit, req_fire);
  Input(bit, out_val);
  Input(bit, out_fire);
  Input(smesh::AccScaleResp, out_bits);
  Input(u3, head_oh);
  Input(u3, tail_oh);
  void updateDrive();
  void updateCheck();
  void reset() override;
  bool passed = true;
 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_).writes(req_val, req_bits, out_rdy, completed_masks);
  UPDATE(updateCheck).reads(cycle_Q_, req_rdy, regs_val, regs_bits, out_regs)
                     .reads(req_fire, out_val, out_fire, out_bits, head_oh, tail_oh)
                     .writes(cycle_D_);
}

void Driver::updateDrive() {
  const auto cycle = static_cast<std::uint8_t>(*cycle_Q_);
  // Fill three slots. Packet 3 waits until cycle 8; packet 4 enters at cycle 11.
  req_val = bit(cycle == 1 || cycle == 2 || cycle == 4 || (cycle >= 5 && cycle <= 8) || cycle == 11);
  req_bits = packet(cycle == 1 ? 0 : cycle == 2 ? 1 : cycle == 4 ? 2 : cycle == 11 ? 4 : 3);
  out_rdy = bit(cycle >= 5 && cycle != 6 && cycle != 7);
  for (std::size_t slot = 0; slot < smesh::AccScaleRegs::kEntries; ++slot) {
    smesh::AccScaleSlotCtrl::CompletedMask mask{};
    for (std::size_t element = 0; element < smesh::AccScaleRegs::kWidth; ++element) {
      // Slots 1 and 2 finish first. Slot 0 lacks its last element at cycle 5.
      mask[element] = bit((slot == 0 && ((cycle >= 6 && cycle <= 8) || cycle == 11 ||
                                        (cycle == 5 && element + 1 < smesh::AccScaleRegs::kWidth))) ||
                          (slot == 1 && ((cycle >= 5 && cycle <= 9) || cycle == 13)) ||
                          (slot == 2 && cycle >= 5));
    }
    completed_masks[slot] = mask;
  }
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<std::uint8_t>(*cycle_Q_);

  struct Expected {
    unsigned valid_mask, head, tail;
    bool ready, input_fire, output_valid, output_fire;
    int stored_packet[3]; // -1 means never written; release retains stored bits
  };
  static const Expected expected[] = {
    // valid/head/tail, ready/in_fire/out_valid/out_fire, stored packet IDs
    {0, 1, 1, 1, 0, 0, 0, {-1, -1, -1}}, // 0: empty
    {0, 1, 1, 1, 1, 0, 0, {-1, -1, -1}}, // 1: accept packet 0
    {1, 1, 2, 1, 1, 0, 0, { 0, -1, -1}}, // 2: accept packet 1
    {3, 1, 4, 1, 0, 0, 0, { 0,  1, -1}}, // 3: idle
    {3, 1, 4, 1, 1, 0, 0, { 0,  1, -1}}, // 4: accept packet 2
    {7, 1, 1, 0, 0, 0, 0, { 0,  1,  2}}, // 5: full, head incomplete
    {7, 1, 1, 0, 0, 1, 0, { 0,  1,  2}}, // 6: head complete, output stalled
    {7, 1, 1, 0, 0, 1, 0, { 0,  1,  2}}, // 7: hold
    {7, 1, 1, 1, 1, 1, 1, { 0,  1,  2}}, // 8: release 0, store 3 in same slot
    {7, 2, 2, 1, 0, 1, 1, { 3,  1,  2}}, // 9: release packet 1
    {5, 4, 2, 1, 0, 1, 1, { 3,  1,  2}}, // 10: release packet 2
    {1, 1, 2, 1, 1, 1, 1, { 3,  1,  2}}, // 11: release 3, store 4 in another slot
    {2, 2, 4, 1, 0, 0, 0, { 3,  4,  2}}, // 12: packet 4 incomplete
    {2, 2, 4, 1, 0, 1, 1, { 3,  4,  2}}, // 13: release packet 4
    {0, 4, 4, 1, 0, 0, 0, { 3,  4,  2}}, // 14: empty despite slot 2's old completion bits
  };
  const auto& e = expected[cycle];
  bool good = (req_rdy == 1) == e.ready && (req_fire == 1) == e.input_fire &&
              (out_val == 1) == e.output_valid && (out_fire == 1) == e.output_fire &&
              head_oh == e.head && tail_oh == e.tail;
  for (std::size_t slot = 0; slot < smesh::AccScaleRegs::kEntries; ++slot) {
    good = good && (regs_val[slot] == 1) == ((e.valid_mask & (1u << slot)) != 0);
    const auto saved = e.stored_packet[slot] < 0 ? smesh::AccScaleReq{} : packet(e.stored_packet[slot]);
    good = good && samePacket(*regs_bits[slot], saved);
    smesh::AccScaleResp expected_output{};
    if (e.stored_packet[slot] >= 0) {
      expected_output.from_dma = saved.norm.acc_read_resp.from_dma;
      expected_output.acc_bank_id = u16(saved.norm.acc_read_resp.laddr.acc_bank());
    }
    good = good && out_regs[slot]->from_dma == expected_output.from_dma &&
           out_regs[slot]->acc_bank_id == expected_output.acc_bank_id &&
           out_regs[slot]->full_data == expected_output.full_data &&
           out_regs[slot]->data == expected_output.data;
    if (e.output_valid && (e.head & (1u << slot)) != 0) {
      good = good && out_bits->from_dma == expected_output.from_dma &&
             out_bits->acc_bank_id == expected_output.acc_bank_id &&
             out_bits->full_data == expected_output.full_data && out_bits->data == expected_output.data;
    }
  }
  s_trace(acc_scale_slot_view_, "cycle=%2u head=%u tail=%u input{r=%u f=%u} output{v=%u f=%u} good=%u\n",
          cycle, static_cast<unsigned>(head_oh), static_cast<unsigned>(tail_oh),
          static_cast<unsigned>(req_rdy), static_cast<unsigned>(req_fire),
          static_cast<unsigned>(out_val), static_cast<unsigned>(out_fire), good);
  if (!good) {
    std::printf("[ACC_SCALE_REGS] mismatch cycle=%u\n", cycle);
    passed = false;
  }
  cycle_D_ = u8(cycle + 1);
}

void Driver::reset() {
  cycle_Q_.reset(0);
  cycle_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::AccScaleReq{});
  out_rdy.reset(0);
  for (std::size_t slot = 0; slot < smesh::AccScaleRegs::kEntries; ++slot) {
    completed_masks[slot].reset(smesh::AccScaleSlotCtrl::CompletedMask{});
  }
  passed = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  smesh::AccScaleRegs regs("Regs");
  smesh::AccScaleSlotCtrl ctrl("SlotCtrl");
  Driver driver("Driver");
  ctrl.req_val << driver.req_val;
  ctrl.out_rdy << driver.out_rdy;
  regs.req_fire << ctrl.req_fire;
  regs.req_bits << driver.req_bits;
  regs.tail_oh << ctrl.tail_oh_Q_;
  regs.out_fire << ctrl.out_fire;
  regs.head_oh << ctrl.head_oh_Q_;
  driver.req_rdy << ctrl.req_rdy;
  driver.req_fire << ctrl.req_fire;
  driver.out_val << ctrl.out_val;
  driver.out_fire << ctrl.out_fire;
  driver.out_bits << regs.out_bits;
  driver.head_oh << ctrl.head_oh_Q_;
  driver.tail_oh << ctrl.tail_oh_Q_;
  for (std::size_t slot = 0; slot < smesh::AccScaleRegs::kEntries; ++slot) {
    ctrl.regs_val[slot] << regs.regs_val_Q_[slot];
    ctrl.completed_masks[slot] << driver.completed_masks[slot];
    driver.regs_val[slot] << regs.regs_val_Q_[slot];
    driver.regs_bits[slot] << regs.regs_bits_Q_[slot];
    driver.out_regs[slot] << regs.out_regs_Q_[slot];
  }
  Clock clk;
  regs.clk << clk;
  ctrl.clk << clk;
  driver.clk << clk;
  clk.generateClock();
  Sim::init();
  bool good = true;
  for (int run = 0; run < 2; ++run) {
    Sim::reset();
    for (int cycle = 0; cycle < (run == 0 ? 8 : 15); ++cycle) Sim::run();
    good = good && driver.passed;
  }
  std::printf("[ACC_SCALE_REGS] slots=%u width=%u %s\n",
              static_cast<unsigned>(smesh::AccScaleRegs::kEntries),
              static_cast<unsigned>(smesh::AccScaleRegs::kWidth), good ? "PASS" : "FAIL");
  descore::flushLog();
  return good ? 0 : 1;
}
