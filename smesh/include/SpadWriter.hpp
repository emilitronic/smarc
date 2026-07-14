// **********************************************************************
// smesh/include/SpadWriter.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/*
Store-side scratchpad writer skeleton.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class SpadWriter : public Component {
  DECLARE_COMPONENT(SpadWriter);

 public:
  SpadWriter(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, req_val);
  Input(StWriterReq, req_bits);
  Output(bit, req_rdy);
  FifoOutput(DmaReadResp, spad_write_out);

  void updateReady();
  void update();
};

} // namespace smesh
