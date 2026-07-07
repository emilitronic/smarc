// **********************************************************************
// smesh/include/DmaReader.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Minimal DMA reader for converting one smesh row request into one memory read.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"
#include "smem/MemTypes.hpp"

namespace smesh {

class DmaReader : public Component {
  DECLARE_COMPONENT(DmaReader);

 public:
  DmaReader(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(DmaReadReq, req_in);
  FifoOutput(smem::MemReq, mem_req);
  FifoInput(smem::MemResp, mem_resp);
  FifoOutput(DmaReadResp, resp_out);

  void updateRequest();
  void updateResponse();
  void reset();

  const DmaReadReq& activeRequest() const { return active_; }

 private:
  bool waiting_ = false;
  DmaReadReq active_{};
};

} // namespace smesh
