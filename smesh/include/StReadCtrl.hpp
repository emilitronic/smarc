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
#include "SmeshTypes.hpp"

namespace smesh {

class StReadCtrl : public Component {
  DECLARE_COMPONENT(StReadCtrl);

 public:
  StReadCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, dispatch_val);          // is dispatch queue signal valid
  Input(DmaWriteReq, dispatch_bits);
  Input(bit, norm_rdy);              // is norm queue signal ready
  InputArray(bit, spad_read_req_rdy, kSpBanks); // spad read req ready, one per bank
  Input(bit, accum_read_req_rdy);    // is accum read req ready
  OutputArray(bit, dmawrite_spad, kSpBanks);
  Output(bit, dmawrite_accum);
  OutputArray(SpadReadReq, spad_req_bits, kSpBanks); // spad read req payload, one per bank
  Output(AccumReadReq, accum_req_bits); // tap off accum read req payload for accum to consume
  Output(bit, read_req_fire);

  void updateReadReq();  //request-valid gen signals to arbiter...
  void updateReadFire(); // ...and read-fire gen based on arbiter readiness (split to avoid combinational cycle)
  void updateInspect();
};

} // namespace smesh
