// **********************************************************************
// smesh/include/mvin/ZeroWriter.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 18 2026
/*
Original-style zero-fill worker: save one load request and emit one masked
local-memory row per accepted response, without using DmaReader or memory.
The saved DmaReadReq is Original's request tag. Responses use our local-write
packet format; only the final row reports the request's entire byte count.
Row repetition and arithmetic scaling are deliberately bypassed. Pixel
repetition belongs to the separate MvinPixelRepeater instance downstream.
*/

#pragma once

#include <cascade/Cascade.hpp>
#include "SmeshPorts.hpp"

namespace smesh {

class ZeroWriter : public Component {
  DECLARE_COMPONENT(ZeroWriter);

 public:
  ZeroWriter(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Input(bit, req_val);
  Input(DmaReadReq, req_bits);
  Output(bit, req_rdy);
  Output(bit, resp_val);
  Output(DmaReadResp, resp_bits);
  Input(bit, resp_rdy);

  void updateResponse();
  void updateReady();
  void updateState();
  void reset();

 private:
  struct State {
    bit valid = 0;
    DmaReadReq request{};
    u16 column = 0;
  };
  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
