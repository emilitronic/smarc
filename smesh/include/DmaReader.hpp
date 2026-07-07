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

  void updateRequest();
  void updateResponse();
  void reset();

  bool hasResponse() const { return response_valid_; }
  const DmaReadReq& activeRequest() const { return active_; }
  std::uint64_t responseData() const { return response_data_; }

 private:
  bool waiting_ = false;
  bool response_valid_ = false;
  DmaReadReq active_{};
  std::uint64_t response_data_ = 0;
};

} // namespace smesh
