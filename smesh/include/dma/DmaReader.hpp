// **********************************************************************
// smesh/include/dma/DmaReader.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
DMA reader for assembling memory beats into one local row request.
Collects multiple memory beats before producing local row data and
produces lane masks.

Currently only starts filling row at lane 0.  Waits for all bytes in current request
before producing output.  Currently limits one request to one accumulator-width row or two
narrow rows.

TODO: for multi-row requests send a completed row to local memory while waiting for
subsequent beats to arrive.
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
  Output(bit, resp_val);
  Output(DmaReadResp, resp_bits);
  Input(bit, resp_rdy);

  void update();
  void updateRespView();
  void reset();

  const DmaReadReq& activeRequest() const { return active_; }

 private:
  struct PendingRows {
    u8 count = 0;
    DmaReadResp first{};
    DmaReadResp second{};
  };

  Output(PendingRows, pending_Q_);
  Register(PendingRows, pending_D_);

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
