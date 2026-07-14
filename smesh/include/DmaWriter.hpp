// **********************************************************************
// smesh/include/DmaWriter.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/*
Store-side DMA writer skeleton.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"
#include "smem/MemTypes.hpp"

namespace smesh {

class DmaWriter : public Component {
  DECLARE_COMPONENT(DmaWriter);

 public:
  DmaWriter(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(DmaWriteReq, issue_in);
  FifoInput(SpadReadResp, spad_data_in);
  FifoInput(AccScaleResp, acc_data_in);
  FifoOutput(smem::MemReq, mem_req);

  void update();
};

} // namespace smesh
