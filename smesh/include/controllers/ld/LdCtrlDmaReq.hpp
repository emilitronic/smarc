// **********************************************************************
// smesh/include/controllers/ld/LdCtrlDmaReq.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026
/*
Builds the DMA request payload and computes the tracker byte count, including accumulator-width,
shrink, zero-stride repeats, and zero-address cases.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

// Builds the load DMA payload and tracker byte count; the FSM drives req.valid.
class LdCtrlDmaReq : public Component {
  DECLARE_COMPONENT(LdCtrlDmaReq);

 public:
  LdCtrlDmaReq(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(u64,            current_vaddr);
  Input(SmeshLocalAddr, current_localaddr);
  Input(u32,            cols);
  Input(u32,            rows);
  Input(u32,            actual_rows_read);
  Input(u64,            stride);
  Input(bit,            all_zeros);

  Input(u32,            scale);
  Input(bit,            shrink);
  Input(u16,            block_stride);
  Input(u8,             pixel_repeat);
  Input(u16,            cmd_id);

  Output(bit,        has_acc_bitwidth);
  Output(u32,        bytes_to_read);
  Output(DmaReadReq, req_bits);

  void updateWidthAndCount();
  void updatePayload();
  void reset();
};

} // namespace smesh
