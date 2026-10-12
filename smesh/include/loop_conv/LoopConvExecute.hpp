// **********************************************************************
// smesh/include/loop_conv/LoopConvExecute.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Generates convolution PRELOAD/COMPUTE pairs and reuses weights across outputs.
Transposed input adds a strides-only CONFIG_EX. Commands traverse two elastic
stages; idle includes their drain. Load-completion inputs gate generation.
*/
#pragma once

#include "LoopConvExecuteReq.hpp"
#include <array>

namespace smesh {

class LoopConvExecute : public Component {
  DECLARE_COMPONENT(LoopConvExecute);
 public:
  LoopConvExecute(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Input(bit,                 req_val);
  Input(LoopConvExecuteReq,  req_bits);
  Output(bit,                req_rdy);
  Output(bit,                cmd_val);
  Output(SmeshCmd,           cmd_bits);
  Input(bit,                 cmd_rdy);
  Input(bit,                 lda_completed);
  Input(bit,                 ldb_completed);
  Input(bit,                 ldd_completed);
  Input(bit,                 ex_utilization_at_limit);
  Output(bit,                idle);
  Output(u8,                 loop_id);

  void updateStatus();
  void updateCommand();
  void updateNextState();
  void reset();
 private:
  enum class Phase : std::uint8_t { Idle, Config, Preload, Compute };
  struct State {
    LoopConvExecuteReq req{};
    Phase phase = Phase::Idle;
    std::uint16_t och  = 0;
    std::uint16_t krow = 0;
    std::uint16_t kcol = 0;
    std::uint16_t kch  = 0;
    std::uint16_t b    = 0;
    std::uint16_t orow = 0;
    std::uint16_t ocol = 0;
    bit new_weights = false;
    std::array<SmeshCmd, 2> commands{};
    std::array<bit, 2> valid{};
  };
  static bool     skipIteration(const State& state);
  static SmeshCmd makeCommand(const State& state);
  Output(State,   state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
