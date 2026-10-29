// **********************************************************************
// smesh/include/dma/DmaWriter.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/* 
Store-side DMA writer; splits a store row into memory beats. Keeps track
of which piece it is sending.  Pieces are held until memory accepts them.
For max pooling, saves element-wise signed maxima across accepted rows and
starts a memory write only when store_en is set. Pooling uses narrow elements.
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

  Input(bit,               req_val);
  Input(StWriterReq,       req_bits);
  Output(bit,              req_rdy);
  FifoOutput(smem::MemReq, mem_req);

  void updateReady();
  void update();
  void reset();

 private:
  Output(StWriterData,   pool_data_Q_);   // running maxima for the current pooling window
  Register(StWriterData, pool_data_D_);
  Output(bit,           active_valid_Q_); // is a row already partway through being sent
  Register(bit,         active_valid_D_);
  Output(StWriterReq,   active_req_Q_);   // DRAM address, row bytes, number of valid bytes
  Register(StWriterReq, active_req_D_);
  Output(u16,           offset_Q_);       // byte position in req data where next mem beat starts
  Register(u16,         offset_D_);
};

} // namespace smesh
