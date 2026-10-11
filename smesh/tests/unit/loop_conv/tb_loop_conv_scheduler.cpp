// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_scheduler.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopConvScheduler.hpp"
#include "SmeshTypes.hpp"

#include <cstdio>

namespace {

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, bool second_has_output, COMPONENT_CTOR);
  Clock(clk);
  Output(smesh::LoopConvDecodedCmd, decoded);
  Output(bit, req_rdy);
  Input(bit, req_val);
  Input(smesh::LoopConvLdBiasReq, req_bits);
  Input(bit, req_fire);
  Input(smesh::LoopConvSlot, loop0);
  Input(smesh::LoopConvSlot, loop1);

  void updateDrive();
  void updateCheck();
  void updateCycle();
  void reset();
  bool passed() const { return passed_ && observed_ == 0xffffu; }

 private:
  const bool second_has_output_;
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  bool passed_ = true;
  unsigned observed_ = 0;
};

Driver::Driver(std::string /*name*/, bool second_has_output, IMPL_CTOR)
    : second_has_output_(second_has_output) {
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_).writes(decoded, req_rdy);
  UPDATE(updateCheck).reads(cycle_Q_, req_val, req_bits, req_fire, loop0, loop1);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
}

// Configure both slots, accepting the first bias job while setting up the second.
void Driver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  smesh::LoopConvDecodedCmd command{};
  if (cycle < 5 || (cycle >= 6 && cycle <= 10)) {
    const auto phase = cycle < 5 ? cycle : cycle - 6;
    const bool second = cycle >= 6;
    command.do_config = 1;
    switch (phase) {
      case 0:
        command.config_id = 2;
        command.inner_bounds.batches = second ? 400 : 2;
        command.inner_bounds.pochs = second ? 9 : 7;
        break;
      case 1:
        command.config_id = 4;
        command.inner_bounds.orows = second ? 400 : 3;
        command.inner_bounds.ocols = second ? 2 : 5;
        break;
      case 2:
        command.config_id = 5;
        command.output_dram_addr = !second || second_has_output_ ? 0x80001000 : 0;
        break;
      case 3:
        command.config_id = 6;
        // Even a zero bias address must be offered: skipping belongs to the worker.
        command.bias_dram_addr = second ? 0x80002000 : 0;
        break;
      case 4:
        command.do_config = 0;
        command.do_launch = 1;
        command.launch.no_bias = bit(!second);
        break;
    }
  }
  decoded = command;
  req_rdy = bit(cycle == 6 || cycle == 12);
}

// Check stalled requests, clocked start flags, address alternation, and no reissue.
void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  if (cycle >= 16) return;
  const auto request = *req_bits;
  const auto first = *loop0;
  const auto second = *loop1;
  const bool offering = cycle == 5 || cycle == 6 || cycle == 11 || cycle == 12;
  const unsigned base = cycle <= 6 ? 0 :
                        cycle <= 12 || !second_has_output_ ? smesh::kAccRows / 2 : 0;
  bool match = req_val == bit(offering) &&
               req_fire == bit(cycle == 6 || cycle == 12) &&
               first.ld_bias_started == bit(cycle >= 7) && first.running == bit(cycle >= 7) &&
               second.ld_bias_started == bit(cycle >= 13) && second.running == bit(cycle >= 13) &&
               request.addr_start == base && request.loop_id == (cycle <= 6 ? 0 : 1);
  if (cycle == 5 || cycle == 6) {
    match &= request.dram_addr == 0 && request.no_bias == 1 &&
             request.ochs == 7 && request.bias_spad_stride == 30 &&
             request.inner_bounds.batches == 2 && request.inner_bounds.orows == 3 &&
             request.inner_bounds.ocols == 5;
  }
  if (cycle == 11 || cycle == 12) {
    match &= request.dram_addr == 0x80002000 && request.no_bias == 0 &&
             request.ochs == 9 && request.bias_spad_stride == 57856 &&
             request.inner_bounds.batches == 400 && request.inner_bounds.orows == 400 &&
             request.inner_bounds.ocols == 2;
  }
  if (!match)
    std::printf("Bias scheduler output=%u cycle=%u valid=%u fire=%u slot=%u base=%u\n",
                unsigned(second_has_output_), cycle, static_cast<unsigned>(*req_val),
                static_cast<unsigned>(*req_fire), unsigned(request.loop_id), request.addr_start);
  passed_ &= match;
  observed_ |= 1u << cycle;
}

void Driver::updateCycle() {
  cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1;
}

void Driver::reset() {
  cycle_D_.reset(0);
  decoded.reset(smesh::LoopConvDecodedCmd{});
  req_rdy.reset(0);
  passed_ = true;
  observed_ = 0;
}

// Exercise the scheduler with the actual state owner and a stand-in bias consumer.
void connect(Driver& driver, smesh::LoopConvState& state,
             smesh::LoopConvScheduler& scheduler, Clock& clk) {
  state.decoded << driver.decoded;
  scheduler.loop0 << state.loop0;
  scheduler.loop1 << state.loop1;
  scheduler.head_loop_id << state.head_loop_id;
  scheduler.ld_bias_req_rdy << driver.req_rdy;
  state.ld_bias_req_fire << scheduler.ld_bias_req_fire;
  state.ld_bias_loop_id << scheduler.ld_bias_loop_id;
  state.ld_bias_idle.wireToConst(bit(0));
  state.ld_bias_worker_loop_id.wireToConst(u8(0));
  driver.req_val << scheduler.ld_bias_req_val;
  driver.req_bits << scheduler.ld_bias_req_bits;
  driver.req_fire << scheduler.ld_bias_req_fire;
  driver.loop0 << state.loop0;
  driver.loop1 << state.loop1;
  driver.clk << clk;
  state.clk << clk;
  scheduler.clk << clk;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver with_output("WithOutput", true), without_output("WithoutOutput", false);
  smesh::LoopConvState state0("State0"), state1("State1");
  smesh::LoopConvScheduler scheduler0("Scheduler0"), scheduler1("Scheduler1");
  Clock clk;
  connect(with_output, state0, scheduler0, clk);
  connect(without_output, state1, scheduler1, clk);
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 16; ++cycle) Sim::run();
  const bool passed = with_output.passed() && without_output.passed();
  descore::flushLog();
  std::printf("[LOOP_CONV_SCHEDULER] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
