// **********************************************************************
// smesh/include/StCtrlDmaReq.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
/*
Store-controller DMA request payload construction.
  It now selects:

  - Normal, pooling, 1-D moveout, or STORE_SPAD virtual address
  - Current or pooling local address
  - Block length and number
  - Normalization command behavior
  - Pooling and final-store flags
  - Activation, scaling, status, normalization, and command ID metadata
  
This block is purely combinational. Request-valid generation, command
acceptance, tracker allocation, and counter updates are handled elsewhere.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class StCtrlDmaReq : public Component {
  DECLARE_COMPONENT(StCtrlDmaReq);

 public:
  StCtrlDmaReq(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, dst_is_spad);
  Input(u32, cols);
  Input(u32, blocks);
  Input(u32, mstatus);

  Input(bit, pooling_is_enabled);
  Input(bit, mvout_1d_enabled);
  Input(u64, current_vaddr);
  Input(SmeshLocalAddr, current_localaddr);
  Input(u64, current_dst_spad_addr);
  Input(SmeshLocalAddr, pool_row_addr);
  Input(u64, pool_vaddr);

  Input(u8, activation);
  Input(u32, acc_scale);
  Input(u32, igelu_qb);
  Input(u32, igelu_qc);
  Input(u32, iexp_qln2);
  Input(u32, iexp_qln2_inv);
  Input(u16, norm_stats_id);

  Input(u32, block_counter);
  Input(u32, wrow_counter);
  Input(u32, wcol_counter);
  Input(u8, pool_size);
  Input(u16, cmd_id);

  Output(DmaWriteReq, req_bits);

  void update();
  void reset();
};

} // namespace smesh
