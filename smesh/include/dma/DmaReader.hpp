// **********************************************************************
// smesh/include/dma/DmaReader.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
DMA reader for assembling memory beats into one local row request.
Collects multiple memory beats before producing local row data and
produces lane masks.

Currently only starts filling row at lane 0 (i.e., first value returned
from memory is placed in first position of that row).  Reader cannot 
currently start placing values at, e.g., lane 2 of a row, and then fill 
lanes 2,3,0,1 in that order.

Waits for all bytes in current request before producing output.  So if the
request asks for 2 narrow rows, the reader will wait for all bytes of both
rows before producing the first output row.

Currently limits one request to one accumulator-width row or two
narrow rows.  If a request for more reaches DmaReader an assertion
stops the simulation.

TODO: for multi-row requests send a completed row to local memory while waiting for
subsequent beats to arrive.

TODO: make DmaReader be able to issue up to 64-byte memory requests (this will
require a BeatMerger capable of re-constructing rows from the (smaller) memory 
beats in which the data is received.
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

  FifoInput(DmaReadReq,    req_in);
  FifoOutput(smem::MemReq, mem_req);
  FifoInput(smem::MemResp, mem_resp);
  Output(bit,         resp_val);
  Output(DmaReadResp, resp_bits);
  Input(bit,          resp_rdy);

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
