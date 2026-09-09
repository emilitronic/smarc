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
  Input(bit, a_valid); // arbiration allows A to proceed
  Input(bit, b_valid);
  Input(bit, d_valid);
  Input(bit, a_ready); // A path is not blocked
  Input(bit, b_ready);
  Input(bit, d_ready);

  Output(bit, firing); // current op needs at least one A/B/D stream to be fed into mesh
  Output(bit, a_fire); // A row-beat handshake, A is accepted for this row-beat
  Output(bit, b_fire); 
  Output(bit, d_fire); 

  void update();
};

} // namespace smesh
