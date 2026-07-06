// **********************************************************************
// smesh/src/LdCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026
/*
Load controller implementation.
*/

#include "LdCtrl.hpp"

namespace smesh {

LdCtrl::LdCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateAccept).reads(cmd_in);
}

void LdCtrl::updateAccept() {
  if (active_valid_ || cmd_in.empty()) {
    return;
  }

  active_ = cmd_in.pop();
  active_valid_ = true;
  trace("ld_ctrl: accepted rob=%u funct=%u",
        static_cast<unsigned>(active_.rob_id),
        static_cast<unsigned>(active_.cmd.funct));
}

void LdCtrl::reset() {
  active_valid_ = false;
  active_ = {};
}

} // namespace smesh
