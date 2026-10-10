// **********************************************************************
// smesh/include/accum_response/AccScaleRegs.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026
/*
Original's three input regs slots and their three matching out_regs slots.
Each input slot holds a complete AccScaleReq packet with kWidth accumulator
elements. Each output slot has space for kWidth full-width results and kWidth
narrowed results, plus the row's source and bank metadata.
Slot control supplies store/release enables and the selected input/output slots.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

// One returning lane result and the output slot/element where it must be stored.
struct AccScaleResult {
  Acc full_data = 0;
  Elem data     = 0;
  u8 slot       = 0;
  u16 element   = 0;
};

class AccScaleRegs : public Component {
  DECLARE_COMPONENT(AccScaleRegs);

 public:
  AccScaleRegs(std::string name, COMPONENT_CTOR);

  static constexpr std::size_t kEntries     = 3;
  static constexpr std::size_t kWidth       = std::tuple_size<MeshAccumRow>::value;
  static constexpr std::size_t kReturnLanes = 4; // four lanes in the current construction
  using CompletedMask                       = std::array<bit, kWidth>;

  Clock(clk);
  Input(bit,           req_fire); // store enable from slot control
  Input(AccScaleReq,   req_bits);

  Input(u3,            tail_oh);  // slot selected for the accepted input
  
  Input(bit,           out_fire); // release enable from slot control
  Input(u3,            head_oh);  // slot selected for output and release
  Output(AccScaleResp, out_bits);

  InputArray(bit,            result_val,  kReturnLanes); // each asserted lane writes one element this cycle
  InputArray(AccScaleResult, result_bits, kReturnLanes);

  // Original regs[s].valid and regs[s].bits, visible after the clock edge.
  // Element e of slot s is regs_bits_Q_[s]->norm.acc_read_resp.data[e].
  OutputArray(bit,         regs_val_Q_,  kEntries);
  OutputArray(AccScaleReq, regs_bits_Q_, kEntries);

  // Original out_regs[s]: full_data, narrowed data, from_dma and acc_bank_id.
  OutputArray(AccScaleResp,  out_regs_Q_,        kEntries);
  OutputArray(CompletedMask, completed_masks_Q_, kEntries); // output elements already written

  void updateOutput();
  void updateRegs();
  void reset() override;

 private:
  RegisterArray(bit,           regs_val_D_,        kEntries);
  RegisterArray(AccScaleReq,   regs_bits_D_,       kEntries);
  RegisterArray(AccScaleResp,  out_regs_D_,        kEntries);
  RegisterArray(CompletedMask, completed_masks_D_, kEntries);
};

} // namespace smesh
