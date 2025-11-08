// **********************************************************************
// smicro/src/MemXbar.hpp
// **********************************************************************
// S Magierowski Aug 16 2025
/*
(Placeholder: not required for the first bring‑up since L2 connects directly to DRAM. 
Add later if you split DRAM vs AccelDRAM or multiple slaves.)
*/

#pragma once
#include <cascade/Cascade.hpp>

class MemXbar : public Component {
  DECLARE_COMPONENT(MemXbar);
public:
  MemXbar(COMPONENT_CTOR);
  Clock(clk);
  void update();
  void reset();
};

