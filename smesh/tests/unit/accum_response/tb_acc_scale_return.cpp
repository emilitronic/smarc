// **********************************************************************
// smesh/tests/unit/accum_response/tb_acc_scale_return.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026
/*
Four lanes return unchanged elements to out_regs. Check assembled rows and
completion masks, younger rows finishing first, output stalls, simultaneous
output/input into a full slot bank, slot reuse, and reset during activity.
The test bridge below stands in for the future scale pipes. Values fit int8,
so full_data and narrowed data represent the same numbers without arithmetic.
*/
// cmake --build build --target tb_acc_scale_return tb_acc_scale_return_dim8 -j 4
// ./build/smesh/tb_acc_scale_return -trace '*'/acc_scale_return_view_

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "AccScaleLane.hpp"
#include "AccScaleSlotCtrl.hpp"

#include <cstdio>
#include <memory>

TraceKey(acc_scale_return_view_);

namespace {

constexpr unsigned kLanes = smesh::AccScaleRegs::kReturnLanes;

smesh::AccScaleReq packet(unsigned id) {
  smesh::AccScaleReq p{};
  auto& row = p.norm.acc_read_resp;
  for (std::size_t e = 0; e < smesh::AccScaleRegs::kWidth; ++e) {
    const int magnitude = (id + 1) * 10 + e;
    row.data[e] = id % 2 == 0 ? magnitude : -magnitude;
  }
  row.laddr = smesh::makeAccAddr((id % smesh::kAccBanks) * smesh::kAccBankRows + id);
  row.from_dma = bit(id % 2);
  return p;
}

bool matches(const smesh::AccScaleResp& row, unsigned id) {
  const auto expected = packet(id).norm.acc_read_resp;
  if (row.from_dma != expected.from_dma || row.acc_bank_id != expected.laddr.acc_bank()) return false;
  for (std::size_t e = 0; e < smesh::AccScaleRegs::kWidth; ++e) {
    if (row.full_data[e] != expected.data[e] || row.data[e] != expected.data[e]) return false;
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
  Output(bit, out_rdy);
  OutputArray(u3, current_policy, kLanes);
  InputArray(bit, lane_val, kLanes);
  InputArray(smesh::AccScaleElem, lane_bits, kLanes);
  OutputArray(bit, result_val, kLanes);
  OutputArray(smesh::AccScaleResult, result_bits, kLanes);
  Input(bit, req_rdy);
  Input(bit, req_fire);
  Input(bit, out_val);
  Input(bit, out_fire);
  Input(smesh::AccScaleResp, out_bits);
  Input(u3, head_oh);
  Input(u3, tail_oh);
  InputArray(smesh::AccScaleRegs::CompletedMask, completed_masks, 3);
  void updateDrive();
  void updateReturn();
  void updateCheck();
  void reset() override;
  bool passed = true;
  unsigned outputs = 0;
  unsigned returns = 0;
  bool saw_partial_head = false;
  bool saw_younger_ready = false;
  bool saw_stall = false;
  bool saw_reuse = false;
 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  Output(u8, sent_Q_);
  Register(u8, sent_D_);
  std::array<smesh::AccScaleRegs::CompletedMask, 3> expected_masks_{};
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  sent_Q_ <= sent_D_;
  UPDATE(updateDrive).reads(cycle_Q_, sent_Q_).writes(req_val, req_bits, out_rdy, current_policy);
  UPDATE(updateReturn).reads(lane_val, lane_bits).writes(result_val, result_bits);
  UPDATE(updateCheck).reads(cycle_Q_, sent_Q_, req_rdy, req_fire, out_val, out_rdy, out_fire)
                     .reads(out_bits, head_oh, tail_oh, completed_masks, result_val, result_bits)
                     .writes(cycle_D_, sent_D_);
}

// Send four packets. Delay lane 0 so some younger output rows finish first.
void Driver::updateDrive() {
  const unsigned c = static_cast<std::uint8_t>(*cycle_Q_);
  const unsigned sent = static_cast<std::uint8_t>(*sent_Q_);
  req_val = bit(sent < 4);
  req_bits = packet(sent < 4 ? sent : 0);
  out_rdy = bit(c >= 17 && c != 20 && c != 21);
  for (unsigned lane = 0; lane < kLanes; ++lane) {
    current_policy[lane] = u3(c >= (lane == 0 ? 8 : 4) ? 7 : 0);
  }
}

// Test-only scale-pipe stand-in: return the registered element without changing its value.
void Driver::updateReturn() {
  for (unsigned lane = 0; lane < kLanes; ++lane) {
    result_val[lane] = *lane_val[lane];
    smesh::AccScaleResult result{};
    if (lane_val[lane] == 1) {
      const auto element = *lane_bits[lane];
      result.slot = element.slot;
      result.element = element.element;
      result.full_data = element.full_data;
      result.data = static_cast<smesh::Elem>(element.data);
    }
    result_bits[lane] = result;
  }
}

// Check current output state before recording this cycle's returns and input capture.
void Driver::updateCheck() {
  const unsigned c = static_cast<std::uint8_t>(*cycle_Q_);
  const unsigned sent = static_cast<std::uint8_t>(*sent_Q_);
  bool good = true;
  std::array<bool, 3> complete{{true, true, true}};
  bool head_has_elements = false;
  for (unsigned slot = 0; slot < 3; ++slot) {
    const auto actual = *completed_masks[slot];
    for (std::size_t e = 0; e < smesh::AccScaleRegs::kWidth; ++e) {
      good = good && actual[e] == expected_masks_[slot][e];
      complete[slot] = complete[slot] && actual[e] == 1;
      if (slot == 0 && actual[e] == 1) head_has_elements = true;
    }
  }
  if (head_oh == 1 && !complete[0]) {
    good = good && out_val == 0;
    if (head_has_elements) saw_partial_head = true;
    if (complete[1] || complete[2]) saw_younger_ready = true;
  }
  if (out_val == 1) {
    good = good && outputs < 4 && matches(*out_bits, outputs);
    if (out_rdy == 0) saw_stall = true;
  }
  if (out_fire == 1) {
    trace(acc_scale_return_view_, "cycle=%02u output row=%u bank=%u from_dma=%u\n",
          c, outputs, static_cast<unsigned>(out_bits->acc_bank_id),
          static_cast<unsigned>(out_bits->from_dma));
    ++outputs;
  }
  if (req_fire == 1) {
    // Same head/tail means a full-bank pop and push reuse the same slot.
    if (out_fire == 1 && head_oh == tail_oh) saw_reuse = true;
    const unsigned slot = sent % 3;
    expected_masks_[slot] = {};
    sent_D_ = u8(sent + 1);
  }
  if (c == 16) good = good && req_rdy == 0; // fourth packet waits while all slots are full
  for (unsigned lane = 0; lane < kLanes; ++lane) {
    if (result_val[lane] == 0) continue;
    const auto result = *result_bits[lane];
    const unsigned slot = static_cast<std::uint8_t>(result.slot);
    const unsigned e = static_cast<std::uint16_t>(result.element);
    good = good && slot < 3 && e < smesh::AccScaleRegs::kWidth;
    if (slot < 3 && e < smesh::AccScaleRegs::kWidth) {
      good = good && expected_masks_[slot][e] == 0;
      expected_masks_[slot][e] = 1;
    }
    ++returns;
    trace(acc_scale_return_view_, "cycle=%02u return lane=%u slot=%u element=%u value=%d\n",
          c, lane, slot, e, static_cast<int>(result.full_data));
  }
  if (!good) std::printf("[ACC_SCALE_RETURN] mismatch cycle=%u\n", c);
  passed = passed && good;
  cycle_D_ = u8(c + 1);
}

void Driver::reset() {
  cycle_Q_.reset(0);
  cycle_D_.reset(0);
  sent_Q_.reset(0);
  sent_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::AccScaleReq{});
  out_rdy.reset(0);
  for (unsigned lane = 0; lane < kLanes; ++lane) {
    current_policy[lane].reset(0);
    result_val[lane].reset(0);
    result_bits[lane].reset(smesh::AccScaleResult{});
  }
  expected_masks_ = {};
  passed = true;
  outputs = 0;
  returns = 0;
  saw_partial_head = saw_younger_ready = saw_stall = saw_reuse = false;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  smesh::AccScaleRegs regs("Regs");
  smesh::AccScaleSlotCtrl ctrl("SlotCtrl");
  std::array<std::unique_ptr<smesh::AccScaleLane>, kLanes> lanes;
  Driver driver("Driver");
  ctrl.req_val << driver.req_val;
  ctrl.out_rdy << driver.out_rdy;
  regs.req_bits << driver.req_bits;
  regs.req_fire << ctrl.req_fire;
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
  for (unsigned slot = 0; slot < 3; ++slot) {
    ctrl.regs_val[slot] << regs.regs_val_Q_[slot];
    ctrl.completed_masks[slot] << regs.completed_masks_Q_[slot];
    driver.completed_masks[slot] << regs.completed_masks_Q_[slot];
  }
  Clock clk;
  regs.clk << clk;
  ctrl.clk << clk;
  driver.clk << clk;
  for (unsigned i = 0; i < kLanes; ++i) {
    lanes[i].reset(new smesh::AccScaleLane("NormLane" + std::to_string(i), i, kLanes, true));
    auto& lane = *lanes[i];
    lane.req_fire << ctrl.req_fire;
    lane.tail_oh << ctrl.tail_oh_Q_;
    lane.current_policy << driver.current_policy[i];
    lane.clk << clk;
    for (unsigned slot = 0; slot < 3; ++slot) {
      lane.regs_val[slot] << regs.regs_val_Q_[slot];
      lane.regs_bits[slot] << regs.regs_bits_Q_[slot];
    }
    driver.lane_val[i] << lane.arb_out_val_Q_;
    driver.lane_bits[i] << lane.arb_out_bits_Q_;
    regs.result_val[i] << driver.result_val[i];
    regs.result_bits[i] << driver.result_bits[i];
  }
  clk.generateClock();
  Sim::init();
  Sim::reset();
  for (int c = 0; c < 8; ++c) Sim::run();
  bool good = driver.passed;
  Sim::reset();
  for (int c = 0; c < 30; ++c) Sim::run();
  good = good && driver.passed && driver.outputs == 4 &&
         driver.returns == 4 * smesh::AccScaleRegs::kWidth && driver.saw_partial_head &&
         driver.saw_younger_ready && driver.saw_stall && driver.saw_reuse;
  std::printf("[ACC_SCALE_RETURN] width=%u rows=%u elements=%u %s\n",
              static_cast<unsigned>(smesh::AccScaleRegs::kWidth), driver.outputs,
              driver.returns, good ? "PASS" : "FAIL");
  descore::flushLog();
  return good ? 0 : 1;
}
