// **********************************************************************
// smesh/src/SmeshShell.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshDevice.hpp"
#include "SmeshMemory.hpp"
#include "SmeshPorts.hpp"

namespace smesh {

class SmeshShell : public Component {
  DECLARE_COMPONENT(SmeshShell);

 public:
  SmeshShell(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SmeshCmd, cmd_in);
  FifoOutput(SmeshResp, resp_out);

  SmeshMemory& memory() { return memory_; }
  const SmeshMemory& memory() const { return memory_; }

  void update();
  void reset();

 private:
  SmeshDevice device_;
  SmeshMemory memory_;
};

} // namespace smesh
