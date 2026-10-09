// **********************************************************************
// smesh/tests/unit/accum_response/tb_acc_scale_lane.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026
/*
Connect one normalization lane to the three input slots. Check its fixed
element connections, policy gating, round-robin order, one-cycle arbOut,
parameter retention, fired masks, slot replacement, and reset during activity.
The driver supplies slot acceptance and current_policy; no arithmetic yet.
*/
// cmake --build build --target tb_acc_scale_lane tb_acc_scale_lane_dim8 -j 4
// ./build/smesh/tb_acc_scale_lane -trace '*'/acc_scale_lane_

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "AccScaleLane.hpp"

#include <cstdio>
#include <utility>
#include <vector>

TraceKey(acc_scale_lane_view_);

namespace {

smesh::AccScaleReq packet(unsigned seed) {
  smesh::AccScaleReq p{};
  auto& row = p.norm.acc_read_resp;
  for (std::size_t e = 0; e < smesh::AccScaleLane::kWidth; ++e) {
    row.data[e] = -static_cast<smesh::Acc>(seed * 100 + e);
  }
  row.scale = 0x3f800000u + seed;
  row.act = 3;
  row.igelu_qb = seed + 10;
  row.igelu_qc = seed + 20;
  row.iexp_qln2 = seed + 30;
  row.iexp_qln2_inv = seed + 40;
  p.norm.mean = seed + 50;
  p.norm.max = seed + 60;
  p.norm.inv_stddev = 0x3f000000u + seed;
  p.norm.inv_sum_exp = 0x3e800000u + seed;
  return p;
}

bool matches(const smesh::AccScaleElem& value, const smesh::AccScaleReq& p,
             unsigned slot, unsigned element) {
  const auto& r = p.norm.acc_read_resp;
  return value.slot == slot && value.element == element &&
         value.data == r.data[element] && value.full_data == r.data[element] &&
         value.scale == r.scale && value.act == r.act &&
         value.igelu_qb == r.igelu_qb && value.igelu_qc == r.igelu_qc &&
         value.iexp_qln2 == r.iexp_qln2 && value.iexp_qln2_inv == r.iexp_qln2_inv &&
         value.mean == p.norm.mean && value.max == p.norm.max &&
         value.inv_stddev == p.norm.inv_stddev && value.inv_sum_exp == p.norm.inv_sum_exp;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, req_fire);
  Output(smesh::AccScaleReq, req_bits);
  Output(u3, tail_oh);
  Output(bit, out_fire);
  Output(u3, head_oh);
  Output(u3, current_policy);
  Input(bit, arb_val);
  Input(smesh::AccScaleElem, arb_bits);
  Input(bit, arb_out_val);
  Input(smesh::AccScaleElem, arb_out_bits);
  InputArray(smesh::AccScaleLane::FiredMask, fired_masks, 3);
  void updateDrive();
  void updateCheck();
  void reset() override;
  bool passed = true;
  unsigned issued = 0;
  unsigned expectedCount() const { return sequence_.size(); }
 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  std::vector<std::pair<unsigned, unsigned>> sequence_;
  std::array<smesh::AccScaleLane::FiredMask, 3> expected_masks_{};
  bool previous_valid_ = false;
  unsigned previous_slot_ = 0;
  unsigned previous_element_ = 0;
  unsigned previous_seed_ = 0;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  // Slot 1 is enabled first. Each lane-0 connection is then chosen exactly once.
  if (smesh::AccScaleLane::kWidth == 4) {
    sequence_ = {{1, 0}, {2, 0}, {0, 0}, {1, 0}};
  } else {
    assert_always(smesh::AccScaleLane::kWidth == 8, "This test expects DIM=4 or DIM=8");
    sequence_ = {{1, 0}, {1, 4}, {2, 0}, {2, 4}, {0, 0}, {0, 4}, {1, 0}, {1, 4}};
  }
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_)
                     .writes(req_fire, req_bits, tail_oh, out_fire, head_oh, current_policy);
  UPDATE(updateCheck).reads(cycle_Q_, arb_val, arb_bits, arb_out_val, arb_out_bits, fired_masks)
                     .writes(cycle_D_);
}

// Fill all slots, enable dispatch, then replace slot 1 with a new packet.
void Driver::updateDrive() {
  const unsigned c = static_cast<std::uint8_t>(*cycle_Q_);
  req_fire = bit(c < 3 || c == 14);
  tail_oh = u3(c < 3 ? 1u << c : 2u);
  req_bits = packet(c == 14 ? 9 : c + 1);
  out_fire = bit(c == 14);
  head_oh = 2;
  current_policy = u3(c < 4 ? 0 : c < 6 ? 2 : 7);
}

// Compare current registered outputs with the preceding cycle's accepted element.
void Driver::updateCheck() {
  const unsigned c = static_cast<std::uint8_t>(*cycle_Q_);
  bool good = (arb_out_val == 1) == previous_valid_;
  if (previous_valid_) {
    good = good && matches(*arb_out_bits, packet(previous_seed_),
                           previous_slot_, previous_element_);
  }
  for (unsigned slot = 0; slot < 3; ++slot) {
    const auto actual = *fired_masks[slot];
    for (std::size_t e = 0; e < smesh::AccScaleLane::kWidth; ++e) {
      good = good && actual[e] == expected_masks_[slot][e];
    }
  }
  const bool accepted = arb_val == 1;
  if (accepted) {
    const auto selected = *arb_bits;
    const unsigned slot = static_cast<std::uint8_t>(selected.slot);
    const unsigned e = static_cast<std::uint16_t>(selected.element);
    const unsigned seed = c >= 15 ? 9 : slot + 1;
    good = good && issued < sequence_.size();
    if (issued < sequence_.size()) {
      good = good && slot == sequence_[issued].first && e == sequence_[issued].second;
    }
    good = good && slot < 3 && e < smesh::AccScaleLane::kWidth;
    if (slot < 3 && e < smesh::AccScaleLane::kWidth) {
      good = good && expected_masks_[slot][e] == 0 &&
             matches(selected, packet(seed), slot, e);
      expected_masks_[slot][e] = 1;
    }
    previous_slot_ = slot;
    previous_element_ = e;
    previous_seed_ = seed;
    ++issued;
    trace(acc_scale_lane_view_, "cycle=%02u accept slot=%u element=%u value=%d\n",
          c, slot, e, static_cast<int>(selected.data));
  }
  // These input handshakes clear fired masks at the next edge, just like regs capture.
  if (c < 3) expected_masks_[c] = {};
  if (c == 14) expected_masks_[1] = {};
  previous_valid_ = accepted;
  if (!good) std::printf("[ACC_SCALE_LANE] mismatch cycle=%u\n", c);
  passed = passed && good;
  cycle_D_ = u8(c + 1);
}

void Driver::reset() {
  cycle_Q_.reset(0);
  cycle_D_.reset(0);
  req_fire.reset(0);
  req_bits.reset(smesh::AccScaleReq{});
  tail_oh.reset(1);
  out_fire.reset(0);
  head_oh.reset(1);
  current_policy.reset(0);
  expected_masks_ = {};
  previous_valid_ = false;
  issued = 0;
  passed = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  smesh::AccScaleRegs regs("Regs");
  smesh::AccScaleLane lane("NormLane0", 0, 4, true);
  Driver driver("Driver");
  regs.req_fire << driver.req_fire;
  regs.req_bits << driver.req_bits;
  regs.tail_oh << driver.tail_oh;
  regs.out_fire << driver.out_fire;
  regs.head_oh << driver.head_oh;
  lane.req_fire << driver.req_fire;
  lane.tail_oh << driver.tail_oh;
  lane.current_policy << driver.current_policy;
  driver.arb_val << lane.arb_val;
  driver.arb_bits << lane.arb_bits;
  driver.arb_out_val << lane.arb_out_val_Q_;
  driver.arb_out_bits << lane.arb_out_bits_Q_;
  for (unsigned slot = 0; slot < 3; ++slot) {
    lane.regs_val[slot] << regs.regs_val_Q_[slot];
    lane.regs_bits[slot] << regs.regs_bits_Q_[slot];
    driver.fired_masks[slot] << lane.fired_masks_Q_[slot];
  }
  Clock clk;
  driver.clk << clk;
  regs.clk << clk;
  lane.clk << clk;
  clk.generateClock();
  Sim::init();
  Sim::reset();
  for (int c = 0; c < 6; ++c) Sim::run();
  bool good = driver.passed;
  Sim::reset();
  for (int c = 0; c < 21; ++c) Sim::run();
  good = good && driver.passed && driver.issued == driver.expectedCount();
  std::printf("[ACC_SCALE_LANE] width=%u issued=%u %s\n",
              static_cast<unsigned>(smesh::AccScaleLane::kWidth), driver.issued,
              good ? "PASS" : "FAIL");
  descore::flushLog();
  return good ? 0 : 1;
}
