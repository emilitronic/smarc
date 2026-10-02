// **********************************************************************
// smesh/include/dma/DmaReader.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
DMA reader for assembling memory beats into local rows and lane masks.
It accepts read requests from the upstream component, and issues memory
requests to the smesh memory interface.  It receives memory responses and
assembles them into local rows, which are sent to the downstream component.

Currently only starts filling row at lane 0 (i.e., first value returned
from memory is placed in first position of that row).  Reader cannot 
currently start placing values at, e.g., lane 2 of a row, and then fill 
lanes 2,3,0,1 in that order.

Sends each completed local row onward while later memory beats are still
arriving. A held beat waits if the output rows cannot advance. One request
may cover up to max(dma_max_bytes, one full accumulator row) bytes.

Requests can span more than two narrow rows (previous limit), up to the
configured byte limit.  Each row keeps its own bytes_read and completion
marker.

TODO: model larger memory transactions returned over multiple response beats.
The current MemReq/MemResp interface transfers one 4- or 8-byte beat per request.
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

  const DmaReadReq& activeRequest() const { return state_Q_->active; }

 private:
  // structure of output buffer for two rows, each with its own completion marker
  struct PendingRows {
    u8          count = 0;
    DmaReadResp first{};
    DmaReadResp second{};
  };
  
  // DmaReader working state
  struct ReaderState {
    bit           active_valid         = 0;
    bit           waiting              = 0; // waiting for a mem_resp for active read req
    DmaReadReq    active{};                 // active read req
    DmaReadData   row_data{};
    std::uint64_t beat_data            = 0; // holds data from one mem resp beat
    std::uint32_t total_bytes          = 0;
    std::uint32_t bytes_requested      = 0; // track active read req: # of B requested
    std::uint32_t bytes_received       = 0; // track active read req: # of B received (i.e. for entire request, not just for a row)
    std::uint16_t row_fill             = 0; // number of B currently accumulated in row_data for row being assembled
    std::uint16_t beat_bytes           = 0;
    std::uint16_t beat_offset          = 0; // track which byte of mem resp beat will be processed by row-loop next
    std::uint16_t requested_beat_bytes = 0;
  };

  Output(PendingRows,   pending_Q_); // current contents of two-row o/p buffer
  Register(PendingRows, pending_D_); // next contents of two-row o/p buffer
  Output(ReaderState,   state_Q_);
  Register(ReaderState, state_D_);
};

} // namespace smesh
