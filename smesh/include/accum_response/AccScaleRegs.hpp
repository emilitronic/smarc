// **********************************************************************
// smesh/include/accum_response/AccScaleRegs.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026
/*
Original's three input regs slots. Each holds one AccScaleReq packet, including
all kWidth accumulator elements and the accompanying settings/statistics.
This first increment captures inputs only; occupied slots remain held until reset.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class AccScaleRegs : public Component {
  DECLARE_COMPONENT(AccScaleRegs);

 public:
  AccScaleRegs(std::string name, COMPONENT_CTOR);

  static constexpr std::size_t kEntries = 3;
  static constexpr std::size_t kWidth   = std::tuple_size<MeshAccumRow>::value;

  Clock(clk);
  Input(bit,         req_val);
  Output(bit,        req_rdy);
  Input(AccScaleReq, req_bits);

  // Original regs[s].valid and regs[s].bits, visible after the clock edge.
  // Element e of slot s is regs_bits_Q_[s]->norm.acc_read_resp.data[e].
  OutputArray(bit,         regs_val_Q_,  kEntries);
  OutputArray(AccScaleReq, regs_bits_Q_, kEntries);

  void updateReady();
  void updateRegs();
  void reset() override;

 private:
  RegisterArray(bit,         regs_val_D_, kEntries);
  RegisterArray(AccScaleReq, regs_bits_D_, kEntries);
  Output(u3,                 tail_oh_Q_); // one set bit selects the next input slot
  Register(u3,               tail_oh_D_);
};

} // namespace smesh
