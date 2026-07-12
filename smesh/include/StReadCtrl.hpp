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

  Input(bit, dispatch_val);          // is dispatch queue signal valid
  Input(DmaWriteReq, dispatch_bits);
  Input(bit, norm_rdy);              // is norm queue signal ready
  Input(bit, spad_read_req_rdy);     // is spad read req ready
  Input(bit, accum_read_req_rdy);    // is accum read req ready
  Output(bit, dmawrite_spad);
  Output(bit, dmawrite_accum);
  Output(bit, read_req_fire);

  void updateReadFire();
  void updateInspect();
};

} // namespace smesh
