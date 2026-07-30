// **********************************************************************
// smesh/include/ExCtrlFeedSignals.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
/*
Small derived control signals for ExecuteController row feeding.
*/

#pragma once

#include <cascade/Cascade.hpp>

namespace smesh {

class ExCtrlFeedSignals : public Component {
  DECLARE_COMPONENT(ExCtrlFeedSignals);

 public:
  ExCtrlFeedSignals(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, start_inputting_a);
  Input(bit, start_inputting_b);
  Input(bit, start_inputting_d);
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
