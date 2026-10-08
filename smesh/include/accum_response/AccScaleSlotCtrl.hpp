// **********************************************************************
// smesh/include/accum_response/AccScaleSlotCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026
/*
Original's ordered slot control. tail_oh selects the next input slot; head_oh
selects the oldest output slot. Only a complete head slot can leave. Its release
can make room for an input in the same cycle, including when all slots are full.
Completion masks come from the element-progress registers, to be added next.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "AccScaleRegs.hpp"

namespace smesh {

class AccScaleSlotCtrl : public Component {
  DECLARE_COMPONENT(AccScaleSlotCtrl);

 public:
  AccScaleSlotCtrl(std::string name, COMPONENT_CTOR);

  static constexpr std::size_t kEntries = AccScaleRegs::kEntries;
  static constexpr std::size_t kWidth = AccScaleRegs::kWidth;
  using CompletedMask = std::array<bit, kWidth>; // one bit per element of a slot

  Clock(clk);
  Input(bit, req_val);
  Output(bit, req_rdy);
  Output(bit, req_fire); // store the input packet in the tail slot
  Input(bit, out_rdy);
  Output(bit, out_val);
  Output(bit, out_fire); // release the head slot

  InputArray(bit, regs_val, kEntries);
  InputArray(CompletedMask, completed_masks, kEntries);
  Output(u3, head_oh_Q_); // one set bit selects the output slot
  Output(u3, tail_oh_Q_); // one set bit selects the input slot

  void updateOutValid();
  void updateOutTransfer();
  void updateReqReady();
  void updateReqTransfer();
  void reset() override;

 private:
  Register(u3, head_oh_D_);
  Register(u3, tail_oh_D_);
};

} // namespace smesh
