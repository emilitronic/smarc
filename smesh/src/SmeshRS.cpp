// **********************************************************************
// smesh/src/SmeshRS.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jun 24 2026
/*
One-entry reservation-station state machine for smesh.
*/

#include "SmeshRS.hpp"

namespace smesh {

bool SmeshRS::empty() const {
  return !entry_.valid;
}

bool SmeshRS::busy() const {
  return entry_.valid;
}

bool SmeshRS::canAccept() const {
  return empty();
}

bool SmeshRS::allocate(const SmeshCmd& cmd) {
  return allocate(cmd, nullptr);
}

bool SmeshRS::allocate(const SmeshCmd& cmd, SmeshRobId* rob_id_out) {
  if (!canAccept()) {
    return false;
  }

  entry_ = SmeshRsEntry{};
  entry_.valid = true;
  entry_.issued = false;
  entry_.queue = classifyCommand(cmd);
  entry_.cmd = cmd;
  entry_.rob_id = next_rob_id_++;

  if (rob_id_out != nullptr) {
    *rob_id_out = entry_.rob_id;
  }
  return true;
}

const SmeshRsEntry& SmeshRS::entry() const {
  return entry_;
}

bool SmeshRS::markIssued(SmeshRobId rob_id) {
  if (!entry_.valid || entry_.rob_id != rob_id) {
    return false;
  }

  entry_.issued = true;
  return true;
}

bool SmeshRS::complete(SmeshRobId rob_id) {
  if (!entry_.valid || entry_.rob_id != rob_id) {
    return false;
  }

  entry_ = SmeshRsEntry{};
  return true;
}

} // namespace smesh
