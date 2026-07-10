// **********************************************************************
// smesh/include/DmaReadCompletionMux.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 9 2026
/*
Mux local-memory load completions back to LdCtrl.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class DmaReadCompletionMux : public Component {
  DECLARE_COMPONENT(DmaReadCompletionMux);

 public:
  DmaReadCompletionMux(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(DmaReadCompletion, spad_in);
  FifoInput(DmaReadCompletion, accum_in);
  FifoOutput(DmaReadCompletion, dma_resp);

  void update();
};

} // namespace smesh
