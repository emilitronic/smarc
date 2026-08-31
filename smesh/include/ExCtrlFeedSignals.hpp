// **********************************************************************
// smesh/include/ExCtrlFeedSignals.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
/*
Small derived control signals for ExecuteController row feeding.

Tells ExCtrlRowFeedState which operand transfers occurred.
The operand handshake signals (*_fire) are also stored in mesh-control packet.
*/

#pragma once

#include <cascade/Cascade.hpp>

namespace smesh {

class ExCtrlFeedSignals : public Component {
  DECLARE_COMPONENT(ExCtrlFeedSignals);

 public:
  ExCtrlFeedSignals(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, start_inputting_a); // FSM indicates that A row-feed stream is active
  Input(bit, start_inputting_b); // FSM indicates that B row-feed stream is active
  Input(bit, start_inputting_d); // FSM indicates that D row-feed stream is active
  Input(bit, a_valid);
  Input(bit, b_valid);
  Input(bit, d_valid);
  Input(bit, a_ready);
  Input(bit, b_ready);
  Input(bit, d_ready);

  Output(bit, firing); // any A/B/D row-feed stream is active
  Output(bit, a_fire); // A row-beat handshake
  Output(bit, b_fire); // B row-beat handshake
  Output(bit, d_fire); // D row-beat handshake

  void update();
};

} // namespace smesh
