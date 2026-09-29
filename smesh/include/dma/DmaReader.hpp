// **********************************************************************
// smesh/include/dma/DmaReader.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Minimal DMA reader for assembling memory beats into one local row request.
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

  void update();
  void reset();

  const DmaReadReq& activeRequest() const { return active_; }

 private:
  bool active_valid_ = false;
  bool waiting_ = false;
  DmaReadReq active_{};
  DmaReadData row_data_{};
  std::uint16_t total_bytes_ = 0;
  std::uint16_t bytes_requested_ = 0;
  std::uint16_t bytes_received_ = 0;
  std::uint16_t beat_bytes_ = 0;
};

} // namespace smesh
