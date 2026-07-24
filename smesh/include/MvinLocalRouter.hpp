// **********************************************************************
// smesh/include/MvinLocalRouter.hpp
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

  FifoInput(DmaReadResp, data_in);
  FifoOutput(DmaReadResp, dmaread_spad);
  FifoOutput(DmaReadResp, dmaread_accum);
  Output(bit, dmaread_spad_val);
  Output(DmaReadResp, dmaread_spad_bits);
  Input(bit, dmaread_spad_rdy);
  Output(bit, dmaread_accum_val);
  Output(DmaReadResp, dmaread_accum_bits);
  Input(bit, dmaread_accum_rdy);

  void update();
  void updateView();
  void reset();

 private:
  bool entry_valid_ = false;
  DmaReadResp entry_{};
};

} // namespace smesh
