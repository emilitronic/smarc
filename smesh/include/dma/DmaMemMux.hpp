// **********************************************************************
// smesh/include/dma/DmaMemMux.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 28 2026
/*
Funnels load-side reader requests and store-side writer requests into a 
single memory request stream, and demultiplexes the responses back to the 
appropriate side.  This is necessary because the smesh memory interface 
only supports one request/response stream, but the DMA has two separate 
streams for reads and writes.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include <cstdint>

#include "smem/MemTypes.hpp"

namespace smesh {

// Shares one external memory port between DMA reads and writes.
class DmaMemMux : public Component {
  DECLARE_COMPONENT(DmaMemMux);

 public:
  DmaMemMux(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoInput(smem::MemReq,   read_req);   // load-side reader req
  FifoOutput(smem::MemResp, read_resp);  // load-side reader resp
  FifoInput(smem::MemReq,   write_req);  // store-side writer req
  FifoOutput(smem::MemReq,  mem_req);    // shared memory req
  FifoInput(smem::MemResp,  mem_resp);   // shared memory resp

  void updateRequest();
  void updateResponse();
};

} // namespace smesh
