// **********************************************************************
// smesh/include/controllers/ex/TransposePreloadUnroller.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 24 2026
/*
Expand a WS transposed-weight PRELOAD/COMPUTE_FLIP pair before ExCtrl's
main command queue. Other commands pass through unchanged.

A two-entry queue retains both original commands while the FSM emits:
  1. PRELOAD with a garbage destination, without a completion tag;
  2. helper COMPUTE_STAY with garbage operands, without a completion tag;
  3. PRELOAD with a garbage source, retaining its original completion tag;
  4. the original COMPUTE_FLIP, unchanged.

Only accepted outputs advance the FSM or remove queued commands. Input ready
depends on current queue occupancy (no same-cycle full-queue replacement).
Reset disables expansion until CONFIG_EX specifies WS and B transpose.

Source clarification: Original assigns the helper's garbage operands to
cmd.inst.rs1/rs2 (instruction register-number fields). Here they are assigned
to cmd.rs1/rs2, the operand payloads. Upper shape bits are preserved.
ExCtrl instantiates this stage ahead of its main command queue by default.
*/
#pragma once

#include <cascade/Cascade.hpp>
#include "SmeshPorts.hpp"

#include <array>

namespace smesh {

class TransposePreloadUnroller : public Component {
  DECLARE_COMPONENT(TransposePreloadUnroller);

 public:
  TransposePreloadUnroller(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Input(bit, in_val);
  Output(bit, in_rdy);
  Input(SmeshIssue, in_bits);
  Output(bit, out_val);
  Input(bit, out_rdy);
  Output(SmeshIssue, out_bits);

  void updateReady();
  void updateOutput();
  void updateState();
  void reset() override;

 private:
  enum class Phase : std::uint8_t { Idle, FirstCompute, SecondPreload };
  struct State {
    std::array<SmeshIssue, 2> entries{};
    u2 count = 0;
    u2 phase = static_cast<std::uint8_t>(Phase::Idle);
    bit transpose_ws = 0;
  };
  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
