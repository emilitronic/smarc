// **********************************************************************
// smesh/include/loop_conv/LoopConvSt.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Generates output STORE commands from accumulator rows after Execute drains.
Ordinary stores use existing StCtrl configuration. Pooling adds CONFIG_STORE
before and after its stores; the final configuration explicitly disables pooling.
Commands traverse two elastic stages; idle includes pipeline drain.
*/
#pragma once

#include "LoopConvCmdDec.hpp"
#include <array>

namespace smesh {

// One loop's output bounds, accumulator base, DRAM destination, and store options.
// ochs is the only field of Original's derived-parameter bundle used by Store.
struct LoopConvStReq {
  LoopConvOuterBounds outer_bounds{};
  LoopConvInnerBounds inner_bounds{};
  std::uint16_t ochs = 0;
  std::uint32_t addr_start = 0;
  std::uint64_t dram_addr = 0;
  bit no_pool = false;
  std::uint8_t activation = 0; // Two-bit activation encoding in Original.
  bit trans_output_1203 = false;
  std::uint8_t loop_id = 0;
};

class LoopConvSt : public Component {
  DECLARE_COMPONENT(LoopConvSt);
 public:
  LoopConvSt(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Input(bit,            req_val);
  Input(LoopConvStReq, req_bits);
  Output(bit,           req_rdy);
  Output(bit,           cmd_val);
  Output(SmeshCmd,      cmd_bits);
  Input(bit,            cmd_rdy);
  Input(bit,            ex_completed);
  Input(bit,            st_utilization_at_limit);
  Output(bit,           idle);
  Output(u8,            loop_id);

  void updateStatus();
  void updateCommand();
  void updateNextState();
  void reset();
 private:
  enum class Phase : std::uint8_t { Idle, Store, PrePoolConfig, Pool, PostPoolConfig };
  struct State {
    LoopConvStReq req{};
    Phase phase = Phase::Idle;
    std::uint16_t b = 0;
    std::uint16_t orow = 0;
    std::uint16_t ocol = 0;
    std::uint16_t och = 0;
    std::array<SmeshCmd, 2> commands{};
    std::array<bit, 2> valid{};
  };
  static SmeshCmd makeCommand(const State& state);
  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
