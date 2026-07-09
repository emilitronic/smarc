// **********************************************************************
// smesh/include/SmeshTop.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 8 2026
/*
Top-level smesh composition point.

This component starts empty on purpose. We will add the RS, controllers, DMA
path, and local memories incrementally so the hardware block structure stays
easy to inspect.
*/

#pragma once

#include <cascade/Cascade.hpp>

namespace smesh {

class SmeshTop : public Component {
  DECLARE_COMPONENT(SmeshTop);

 public:
  SmeshTop(std::string name, COMPONENT_CTOR);

  Clock(clk);

  void update();
  void reset();
};

} // namespace smesh
