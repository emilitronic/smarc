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

  void update();
};

} // namespace smesh
