// **********************************************************************
// smesh/include/mvin/MvinLocalRouter.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 9 2026
/*
Route load-path write data to scratchpad or accumulator by local-address type.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class MvinLocalRouter : public Component {
  DECLARE_COMPONENT(MvinLocalRouter);

 public:
  MvinLocalRouter(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, in_val);
  Input(DmaReadResp, in_bits);
  Output(bit, in_rdy);
  Output(bit, dmaread_spad_val);
  Output(DmaReadResp, dmaread_spad_bits);
  Input(bit, dmaread_spad_rdy);
  Output(bit, dmaread_accum_val);
  Output(DmaReadResp, dmaread_accum_bits);
  Input(bit, dmaread_accum_rdy);

  void updateView();
  void updateReady();
  void updateStorage();
  void reset();

 private:
  struct Entry {
    bit valid = 0;
    DmaReadResp bits{};
  };

  Output(Entry, entry_Q_);
  Register(Entry, entry_D_);
};

} // namespace smesh
