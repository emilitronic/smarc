// **********************************************************************
// smesh/tests/unit/accum_response/tb_acc_scale_lane.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026
/*
Connect one, two, or four normalization lanes to the three input slots. Check their fixed
element connections, policy gating, round-robin order, one-cycle arbOut,
parameter retention, fired masks, slot replacement, and reset during activity.
The driver supplies slot acceptance and current_policy; no arithmetic yet.
*/
// cmake --build build --target tb_acc_scale_lane tb_acc_scale_lane_dim8 -j 4
// ./build/smesh/tb_acc_scale_lane -trace '*'/acc_scale_lane_
// cmake --build build --target tb_acc_scale_two_lanes tb_acc_scale_two_lanes_dim8 -j 4
// ./build/smesh/tb_acc_scale_two_lanes -trace '*'/acc_scale_lane_view_
// cmake --build build --target tb_acc_scale_four_lanes tb_acc_scale_four_lanes_dim8 -j 4
// ./build/smesh/tb_acc_scale_four_lanes -trace '*'/acc_scale_lane_view_

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "AccScaleLane.hpp"

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

TraceKey(acc_scale_lane_view_);

namespace {

#ifndef SMESH_ACC_SCALE_TEST_LANES
#define SMESH_ACC_SCALE_TEST_LANES 1
#endif
constexpr unsigned kLanes = SMESH_ACC_SCALE_TEST_LANES;
static_assert(kLanes == 1 || kLanes == 2 || kLanes == 4,
              "This test uses one, two, or all four lanes");

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
  OutputArray(u3, current_policy, kLanes);
  InputArray(bit, arb_val, kLanes);
  InputArray(smesh::AccScaleElem, arb_bits, kLanes);
  InputArray(bit, arb_out_val, kLanes);
  InputArray(smesh::AccScaleElem, arb_out_bits, kLanes);
  InputArray(smesh::AccScaleLane::FiredMask, fired_masks, kLanes * 3);
  void updateDrive();
  void updateCheck();
  void reset() override;
  bool passed = true;
  std::array<unsigned, kLanes> issued{};
  unsigned simultaneous = 0;
  unsigned expectedCount(unsigned lane) const { return sequence_[lane].size(); }
 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  std::array<std::vector<std::pair<unsigned, unsigned>>, kLanes> sequence_;
  std::array<std::array<smesh::AccScaleLane::FiredMask, 3>, kLanes> expected_masks_{};
  std::array<bool, kLanes> previous_valid_{};
  std::array<unsigned, kLanes> previous_slot_{};
  std::array<unsigned, kLanes> previous_element_{};
  std::array<unsigned, kLanes> previous_seed_{};
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  assert_always(smesh::AccScaleLane::kWidth == 4 || smesh::AccScaleLane::kWidth == 8,
                "This test expects DIM=4 or DIM=8");
  for (unsigned lane = 0; lane < kLanes; ++lane) {
    // Enable slot 1 first for lane 0 and slot 2 first for the other lanes, then all slots.
    // Different winners check that the lanes keep separate round-robin memories.
    const unsigned first = lane == 0 ? 1 : 2;
    for (unsigned turn = 0; turn < 3; ++turn) {
      const unsigned slot = (first + turn) % 3;
      sequence_[lane].emplace_back(slot, lane);
      if (smesh::AccScaleLane::kWidth == 8) sequence_[lane].emplace_back(slot, lane + 4);
    }
    sequence_[lane].emplace_back(1, lane);
    if (smesh::AccScaleLane::kWidth == 8) sequence_[lane].emplace_back(1, lane + 4);
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
  for (unsigned lane = 0; lane < kLanes; ++lane) {
    current_policy[lane] = u3(c < 4 ? 0 : c < 6 ? (lane == 0 ? 2 : 4) : 7);
  }
}

// Compare current registered outputs with the preceding cycle's accepted element.
void Driver::updateCheck() {
  const unsigned c = static_cast<std::uint8_t>(*cycle_Q_);
  bool good = true;
  bool all_accepted = true;
  for (unsigned lane = 0; lane < kLanes; ++lane) {
    good = good && (arb_out_val[lane] == 1) == previous_valid_[lane];
    if (previous_valid_[lane]) {
      good = good && matches(*arb_out_bits[lane], packet(previous_seed_[lane]),
                             previous_slot_[lane], previous_element_[lane]);
    }
    for (unsigned slot = 0; slot < 3; ++slot) {
      const auto actual = *fired_masks[lane * 3 + slot];
      for (std::size_t e = 0; e < smesh::AccScaleLane::kWidth; ++e) {
        good = good && actual[e] == expected_masks_[lane][slot][e];
      }
    }
    const bool accepted = arb_val[lane] == 1;
    all_accepted = all_accepted && accepted;
    if (accepted) {
      const auto selected = *arb_bits[lane];
      const unsigned slot = static_cast<std::uint8_t>(selected.slot);
      const unsigned e = static_cast<std::uint16_t>(selected.element);
      const unsigned seed = c >= 15 ? 9 : slot + 1;
      good = good && issued[lane] < sequence_[lane].size();
      if (issued[lane] < sequence_[lane].size()) {
        good = good && slot == sequence_[lane][issued[lane]].first &&
               e == sequence_[lane][issued[lane]].second;
      }
      good = good && slot < 3 && e < smesh::AccScaleLane::kWidth;
      if (slot < 3 && e < smesh::AccScaleLane::kWidth) {
        good = good && expected_masks_[lane][slot][e] == 0 &&
               matches(selected, packet(seed), slot, e);
        expected_masks_[lane][slot][e] = 1;
      }
      previous_slot_[lane] = slot;
      previous_element_[lane] = e;
      previous_seed_[lane] = seed;
      ++issued[lane];
      trace(acc_scale_lane_view_, "cycle=%02u lane=%u accept slot=%u element=%u value=%d\n",
            c, lane, slot, e, static_cast<int>(selected.data));
    }
    // These input handshakes clear fired masks at the next edge, just like regs capture.
    if (c < 3) expected_masks_[lane][c] = {};
    if (c == 14) expected_masks_[lane][1] = {};
    previous_valid_[lane] = accepted;
  }
  // With all four lanes, every element must have been sent by exactly one lane.
  // Check the original rows and again after slot 1 has received a replacement.
  if (kLanes == 4 && (c == 13 || c == 20)) {
    for (unsigned slot = 0; slot < 3; ++slot) {
      for (std::size_t e = 0; e < smesh::AccScaleLane::kWidth; ++e) {
        unsigned senders = 0;
        for (unsigned lane = 0; lane < kLanes; ++lane) {
          senders += (*fired_masks[lane * 3 + slot])[e] == 1 ? 1 : 0;
        }
        good = good && senders == 1;
      }
    }
  }
  if (all_accepted) ++simultaneous;
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
  for (unsigned lane = 0; lane < kLanes; ++lane) current_policy[lane].reset(0);
  expected_masks_ = {};
  previous_valid_ = {};
  issued = {};
  simultaneous = 0;
  passed = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  smesh::AccScaleRegs regs("Regs");
  std::array<std::unique_ptr<smesh::AccScaleLane>, kLanes> lanes;
  Driver driver("Driver");
  regs.req_fire << driver.req_fire;
  regs.req_bits << driver.req_bits;
  regs.tail_oh << driver.tail_oh;
  regs.out_fire << driver.out_fire;
  regs.head_oh << driver.head_oh;
  for (unsigned i = 0; i < kLanes; ++i) {
    lanes[i].reset(new smesh::AccScaleLane("NormLane" + std::to_string(i), i, 4, true));
    auto& lane = *lanes[i];
    lane.req_fire << driver.req_fire;
    lane.tail_oh << driver.tail_oh;
    lane.current_policy << driver.current_policy[i];
    driver.arb_val[i] << lane.arb_val;
    driver.arb_bits[i] << lane.arb_bits;
    driver.arb_out_val[i] << lane.arb_out_val_Q_;
    driver.arb_out_bits[i] << lane.arb_out_bits_Q_;
    for (unsigned slot = 0; slot < 3; ++slot) {
      lane.regs_val[slot] << regs.regs_val_Q_[slot];
      lane.regs_bits[slot] << regs.regs_bits_Q_[slot];
      driver.fired_masks[i * 3 + slot] << lane.fired_masks_Q_[slot];
    }
  }
  Clock clk;
  driver.clk << clk;
  regs.clk << clk;
  for (auto& lane : lanes) lane->clk << clk;
  clk.generateClock();
  Sim::init();
  Sim::reset();
  for (int c = 0; c < 6; ++c) Sim::run();
  bool good = driver.passed;
  Sim::reset();
  for (int c = 0; c < 21; ++c) Sim::run();
  good = good && driver.passed && driver.simultaneous > 0;
  unsigned total_issued = 0;
  for (unsigned lane = 0; lane < kLanes; ++lane) {
    good = good && driver.issued[lane] == driver.expectedCount(lane);
    total_issued += driver.issued[lane];
  }
  std::printf("[ACC_SCALE_LANE] lanes=%u width=%u issued=%u simultaneous=%u %s\n",
              kLanes, static_cast<unsigned>(smesh::AccScaleLane::kWidth), total_issued,
              driver.simultaneous,
              good ? "PASS" : "FAIL");
  descore::flushLog();
  return good ? 0 : 1;
}
