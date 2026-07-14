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

  Input(bit, req_val);
  Input(StWriterReq, req_bits);
  Output(bit, req_rdy);
  FifoOutput(smem::MemReq, mem_req);

  void updateReady();
  void update();
};

} // namespace smesh
