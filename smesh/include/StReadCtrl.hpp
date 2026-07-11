// **********************************************************************
// smesh/include/StReadCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 11 2026
/*
Store-path local-memory read control.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class StReadCtrl : public Component {
  DECLARE_COMPONENT(StReadCtrl);

 public:
  StReadCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, dispatch_valid);        // is dispatch queue signal valid
  Input(DmaWriteReq, dispatch_bits);
  Input(bit, norm_ready);            // is norm queue signal ready
  Output(bit, dispatch_ready);

  void updateReady();
  void updateInspect();
};

} // namespace smesh
