// **********************************************************************
// smesh/include/StIssueCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 14 2026
/*
Store-path final issue control skeleton.
*/

#pragma once

#include <cascade/Cascade.hpp>

namespace smesh {

class StIssueCtrl : public Component {
  DECLARE_COMPONENT(StIssueCtrl);

 public:
  StIssueCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  void update();
};

} // namespace smesh
