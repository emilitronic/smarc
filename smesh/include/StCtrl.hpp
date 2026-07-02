// **********************************************************************
// smesh/include/StCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026
/*
Skeleton for the smesh store controller.
*/
#pragma once

#include <cascade/Cascade.hpp>

namespace smesh {

class StCtrl : public Component {
  DECLARE_COMPONENT(StCtrl);

 public:
  StCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);
};

} // namespace smesh
