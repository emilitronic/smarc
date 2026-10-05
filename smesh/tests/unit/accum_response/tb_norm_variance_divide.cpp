// **********************************************************************
// smesh/tests/unit/accum_response/tb_norm_variance_divide.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026
/*
Focused GetVariance test for the Normalizer.  Checks that slot 1's 43/4 produces
10 after the divider delay, saves variance[1], and leaves slot 0 unchanged.
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "NormMeanDivide.hpp"
#include "NormStats.hpp"

#include <cstdint>
#include <cstdio>

namespace {

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(smesh::NormStateRegs, slot_states);
  Output(bit, accept_val);
  Output(u8, accept_id);
  Output(smesh::AccNormReq, req_bits);
  Output(bit, sum_val);
  Output(smesh::NormSumResult, sum_bits);
  Output(bit, max_val);
  Output(smesh::NormMaxResult, max_bits);
  Output(bit, sum_chunk_val);
  Output(smesh::NormChunk, sum_chunk_bits);
  Output(bit, max_chunk_val);
  Output(smesh::NormChunk, max_chunk_bits);

  Input(bit, started);
  Input(u8, start_id);
  Input(bit, finished);
  Input(u8, finish_id);
  Input(smesh::Acc, result);
  Input(smesh::NormStatsRegs, stats);

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
  UPDATE(updateDrive).reads(phase_Q_)
                     .writes(slot_states, accept_val, accept_id, req_bits,
                             sum_val, sum_bits, max_val, max_bits)
                     .writes(sum_chunk_val, sum_chunk_bits, max_chunk_val, max_chunk_bits);
  UPDATE(updateCheck).reads(phase_Q_, started, start_id, finished, finish_id,
                             result, stats).writes(phase_D_);
}

void Driver::updateDrive() {
  const auto phase = static_cast<std::uint8_t>(*phase_Q_);
  smesh::NormStateRegs states{};
  if (phase == 2) states.state[1] = u8(static_cast<std::uint8_t>(smesh::NormFsmState::GetVariance));
  if (phase == 3) states.state[1] = u8(static_cast<std::uint8_t>(smesh::NormFsmState::WaitingForVariance));
  if (phase == 4) states.state[1] = u8(static_cast<std::uint8_t>(smesh::NormFsmState::GetStddev));
  slot_states = states;

  accept_val = bit(phase <= 1);
  accept_id = u8(phase == 1 ? 1 : 0);
  smesh::AccNormReq req{};
  req.cmd.cmd = static_cast<std::uint8_t>(phase == 1 ? smesh::NormCmd::Variance : smesh::NormCmd::Sum);
  req.cmd.len = phase == 1 ? 4 : 2;
  req_bits = req;

  sum_val = bit(phase <= 1);
  sum_bits = smesh::NormSumResult{u8(phase == 1 ? 1 : 0), u32(phase == 1 ? 43 : 14)};
  max_val = 0;
  max_bits = smesh::NormMaxResult{};
  sum_chunk_val = 0;
  sum_chunk_bits = smesh::NormChunk{};
  max_chunk_val = 0;
  max_chunk_bits = smesh::NormChunk{};
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto phase = static_cast<std::uint8_t>(*phase_Q_);
  const auto values = *stats;
  bool good = true;

  switch (phase) {
    case 0: good = started == 0 && finished == 0; break;
    case 1: good = started == 0 && finished == 0 && values.sum[0] == 14 && values.count[0] == 2; break;
    case 2: good = started == 1 && start_id == 1 && finished == 0 &&
                   values.sum[1] == 43 && values.count[1] == 4; break;
    case 3: good = started == 0 && finished == 1 && finish_id == 1 && result == 10 &&
                   values.sum[1] == 0 && values.count[1] == 0 && values.variance[1] == 0; break;
    case 4: good = started == 0 && finished == 0 && values.variance[1] == 10 &&
                   values.mean[1] == 0 && values.sum[0] == 14 && values.count[0] == 2;
            done_ = true; break;
    default: break;
  }
  if (!good) {
    std::printf("[NORM_VARIANCE_DIVIDE] phase=%u start=%u finish=%u result=%d sum1=%u count1=%u variance1=%d\n",
                static_cast<unsigned>(phase), static_cast<unsigned>(started == 1),
                static_cast<unsigned>(finished == 1), static_cast<int>(*result),
                static_cast<unsigned>(values.sum[1]), static_cast<unsigned>(values.count[1]),
                static_cast<int>(values.variance[1]));
    passed_ = false;
  }
  if (!done_) phase_D_ = u8(phase + 1);
}

void Driver::reset() {
  phase_Q_.reset(0);
  phase_D_.reset(0);
  slot_states.reset(smesh::NormStateRegs{});
  accept_val.reset(0);
  accept_id.reset(0);
  req_bits.reset(smesh::AccNormReq{});
  sum_val.reset(0);
  sum_bits.reset(smesh::NormSumResult{});
  max_val.reset(0);
  max_bits.reset(smesh::NormMaxResult{});
  sum_chunk_val.reset(0);
  sum_chunk_bits.reset(smesh::NormChunk{});
  max_chunk_val.reset(0);
  max_chunk_bits.reset(smesh::NormChunk{});
  done_ = false;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::NormStats stats("Stats");
  smesh::NormMeanDivide divide("Divide");
  Driver driver("Driver");

  stats.slot_states << driver.slot_states;
  stats.accept_val << driver.accept_val;
  stats.accept_id << driver.accept_id;
  stats.req_bits << driver.req_bits;
  stats.sum_val << driver.sum_val;
  stats.sum_bits << driver.sum_bits;
  stats.max_val << driver.max_val;
  stats.max_bits << driver.max_bits;
  stats.sum_chunk_val << driver.sum_chunk_val;
  stats.sum_chunk_bits << driver.sum_chunk_bits;
  stats.max_chunk_val << driver.max_chunk_val;
  stats.max_chunk_bits << driver.max_chunk_bits;
  stats.divide_started << divide.started;
  stats.divide_start_id << divide.start_id;
  stats.divide_finished << divide.finished;
  stats.divide_finish_id << divide.finish_id;
  stats.divide_result << divide.result;

  divide.slot_states << driver.slot_states;
  divide.stats << stats.view;
  driver.started << divide.started;
  driver.start_id << divide.start_id;
  driver.finished << divide.finished;
  driver.finish_id << divide.finish_id;
  driver.result << divide.result;
  driver.stats << stats.view;

  Clock clk;
  stats.clk << clk;
  divide.clk << clk;
  driver.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 8 && !driver.done(); ++cycle) Sim::run();

  const bool ok = driver.done() && driver.passed();
  std::printf("[NORM_VARIANCE_DIVIDE] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
