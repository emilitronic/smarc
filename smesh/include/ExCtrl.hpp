// **********************************************************************
// smesh/include/ExCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026
/*
Skeleton for the smesh execute controller.
*/
#pragma once

#include <cascade/Cascade.hpp>

namespace smesh {

class ExCtrl : public Component {
  DECLARE_COMPONENT(ExCtrl);

 public:
  ExCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);
};

} // namespace smesh
