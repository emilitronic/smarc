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
  return !load_entry_.valid && !execute_entry_.valid && !store_entry_.valid;
}

bool SmeshRS::busy() const {
  return !empty();
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

  SmeshRsEntry* slot = nullptr;
  const auto queue = classifyCommand(cmd);
  switch (queue) {
    case SmeshQueueClass::Load:
      slot = &load_entry_;    // choose the load row
      break;
    case SmeshQueueClass::Execute:
      slot = &execute_entry_; // choose the execute row
      break;
    case SmeshQueueClass::Store:
      slot = &store_entry_;   // choose the store row
      break;
    case SmeshQueueClass::System:
    case SmeshQueueClass::Invalid:
      return false;
  }

  *slot = SmeshRsEntry{};  // clear chosen row before filling it
  slot->valid = true;
  slot->issued = false;
  slot->queue = queue;
  slot->cmd = cmd;
  slot->rob_id = next_rob_id_++;

  if (rob_id_out != nullptr) {
    *rob_id_out = slot->rob_id;
  }
  return true;
}

// which entry is currently occupied
const SmeshRsEntry& SmeshRS::entry() const {
  if (load_entry_.valid) {
    return load_entry_;
  }
  if (execute_entry_.valid) {
    return execute_entry_;
  }
  if (store_entry_.valid) {
    return store_entry_;
  }
  return execute_entry_;
}

// read load RS row
const SmeshRsEntry& SmeshRS::loadEntry() const {
  return load_entry_;
}
// read execute RS row
const SmeshRsEntry& SmeshRS::executeEntry() const {
  return execute_entry_;
}
// read store RS row
const SmeshRsEntry& SmeshRS::storeEntry() const {
  return store_entry_;
}

bool SmeshRS::markIssued(SmeshRobId rob_id) {
  for (auto* entry : {&load_entry_, &execute_entry_, &store_entry_}) {
    if (entry->valid && entry->rob_id == rob_id) {
      entry->issued = true;
      return true;
    }
  }

  return false;
}

bool SmeshRS::complete(SmeshRobId rob_id) {
  for (auto* entry : {&load_entry_, &execute_entry_, &store_entry_}) {
    if (entry->valid && entry->rob_id == rob_id) {
      *entry = SmeshRsEntry{};
      return true;
    }
  }

  return false;
}

} // namespace smesh
