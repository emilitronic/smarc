// **********************************************************************
// smesh/tests/unit/accum_response/tb_acc_scale_work_class.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 9 2026
/*
Check work classification for every combination of three valid bits and three
activation fields (None, ReLU, LayerNorm, IGELU, softmax). Compare five lane
splits against explicit policy tables, including all-normalization and
all-ordinary constructions. Invalid slots must not set norm_mask; policy may
still assign them to a group. No arithmetic or mixed lane wiring is tested.
*/
// cmake --build build --target tb_acc_scale_work_class -j 4
// ./build/smesh/tb_acc_scale_work_class -trace '*'/acc_scale_work_class_view_

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "AccScaleWorkClass.hpp"

#include <array>
#include <cstdio>
#include <memory>

TraceKey(acc_scale_work_class_view_);

namespace {

constexpr unsigned kCases = 8 * 5 * 5 * 5;
constexpr unsigned kSplits = 5;
constexpr std::array<unsigned, kSplits> kTotal{{4, 8, 6, 12, 4}};
constexpr std::array<unsigned, kSplits> kNorm{{4, 4, 4, 4, 0}};
// Index is norm_mask; bit s in the value selects the norm lane group for slot s.
constexpr std::array<std::array<unsigned, 8>, kSplits> kPolicy{{
    {{7, 7, 7, 7, 7, 7, 7, 7}},
    {{6, 5, 6, 3, 6, 5, 6, 7}},
    {{6, 5, 6, 3, 6, 5, 6, 7}},
    {{4, 1, 2, 3, 4, 5, 6, 7}},
    {{0, 0, 0, 0, 0, 0, 0, 0}}
}};

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  OutputArray(bit, regs_val, 3);
  OutputArray(smesh::AccScaleReq, regs_bits, 3);
  InputArray(u3, norm_mask, kSplits);
  InputArray(u3, current_policy, kSplits);
  void updateDrive();
  void updateCheck();
  void reset() override;
  bool passed = true;
  unsigned checked = 0;

 private:
  Output(u16, case_Q_);
  Register(u16, case_D_);
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  case_Q_ <= case_D_;
  UPDATE(updateDrive).reads(case_Q_).writes(regs_val, regs_bits);
  UPDATE(updateCheck).reads(case_Q_, norm_mask, current_policy).writes(case_D_);
}

void Driver::updateDrive() {
  const unsigned id = static_cast<std::uint16_t>(*case_Q_);
  unsigned acts = id / 8;
  for (unsigned slot = 0; slot < 3; ++slot) {
    smesh::AccScaleReq row{};
    row.norm.acc_read_resp.act = u8(acts % 5);
    acts /= 5;
    regs_val[slot] = bit((id >> slot) & 1u);
    regs_bits[slot] = row;
  }
}

void Driver::updateCheck() {
  const unsigned id = static_cast<std::uint16_t>(*case_Q_);
  if (id >= kCases) return;
  unsigned acts = id / 8;
  unsigned expected_mask = 0;
  for (unsigned slot = 0; slot < 3; ++slot) {
    const auto act = acts % 5;
    acts /= 5;
    if (((id >> slot) & 1u) && act >= 2) expected_mask |= 1u << slot;
  }
  for (unsigned split = 0; split < kSplits; ++split) {
    const unsigned mask = static_cast<std::uint8_t>(*norm_mask[split]);
    const unsigned policy = static_cast<std::uint8_t>(*current_policy[split]);
    const bool good = mask == expected_mask && policy == kPolicy[split][expected_mask];
    if (!good) {
      std::printf("[ACC_SCALE_WORK_CLASS] case=%u lanes=%u norm=%u mask=%u expected=%u policy=%u expected=%u\n",
                  id, kTotal[split], kNorm[split], mask, expected_mask,
                  policy, kPolicy[split][expected_mask]);
    }
    passed = passed && good;
    trace(acc_scale_work_class_view_, "case=%03u lanes=%02u norm=%u norm_mask=%u policy=%u\n",
          id, kTotal[split], kNorm[split], mask, policy);
  }
  ++checked;
  case_D_ = u16(id + 1);
}

void Driver::reset() {
  case_Q_.reset(0);
  case_D_.reset(0);
  for (unsigned slot = 0; slot < 3; ++slot) {
    regs_val[slot].reset(0);
    regs_bits[slot].reset(smesh::AccScaleReq{});
  }
  passed = true;
  checked = 0;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver driver("Driver");
  Clock clk;
  std::array<std::unique_ptr<smesh::AccScaleWorkClass>, kSplits> classifiers;
  for (unsigned split = 0; split < kSplits; ++split) {
    classifiers[split].reset(new smesh::AccScaleWorkClass("WorkClass" + std::to_string(split),
                                                        kTotal[split], kNorm[split]));
    classifiers[split]->clk << clk;
    for (unsigned slot = 0; slot < 3; ++slot) {
      classifiers[split]->regs_val[slot] << driver.regs_val[slot];
      classifiers[split]->regs_bits[slot] << driver.regs_bits[slot];
    }
    driver.norm_mask[split] << classifiers[split]->norm_mask;
    driver.current_policy[split] << classifiers[split]->current_policy;
  }
  driver.clk << clk;
  clk.generateClock();
  Sim::init();
  Sim::reset();
  bool good = true;
  for (unsigned split = 0; split < kSplits; ++split) {
    good = good && classifiers[split]->norm_mask == 0 &&
           classifiers[split]->current_policy == kPolicy[split][0];
  }
  for (unsigned cycle = 0; cycle < 17; ++cycle) Sim::run();
  good = good && driver.passed;
  Sim::reset();
  for (unsigned cycle = 0; cycle < kCases + 1; ++cycle) Sim::run();
  good = good && driver.passed && driver.checked == kCases;
  std::printf("[ACC_SCALE_WORK_CLASS] cases=%u splits=%u %s\n",
              driver.checked, kSplits, good ? "PASS" : "FAIL");
  descore::flushLog();
  return good ? 0 : 1;
}
