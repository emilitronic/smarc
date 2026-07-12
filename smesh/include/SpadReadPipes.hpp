// **********************************************************************
// smesh/include/SpadReadPipes.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
Scratchpad read response pipes.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class SpadDmaReadPipe : public Component {
  DECLARE_COMPONENT(SpadDmaReadPipe);

 public:
  SpadDmaReadPipe(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SpadReadResp, resp_in);
  FifoOutput(SpadReadResp, resp_out);

  void update();
};

class ExDmaReadPipe : public Component {
  DECLARE_COMPONENT(ExDmaReadPipe);

 public:
  ExDmaReadPipe(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SpadReadResp, resp_in);
  FifoOutput(SpadReadResp, resp_out);

  void update();
};

} // namespace smesh
