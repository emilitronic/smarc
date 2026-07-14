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

  FifoInput(DmaWriteReq, issue_in);
  FifoInput(SpadReadResp, spad_data_in);
  FifoInput(AccScaleResp, acc_data_in);
  FifoOutput(DmaReadResp, spad_write_out);

  void update();
};

} // namespace smesh
