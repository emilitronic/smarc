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
  return !entries_ld_.valid && !entries_ex_.valid && !entries_st_.valid;
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
      slot = &entries_ld_;    // choose the load row
      break;
    case SmeshQueueClass::Execute:
      slot = &entries_ex_;    // choose the execute row
      break;
    case SmeshQueueClass::Store:
      slot = &entries_st_;    // choose the store row
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
  if (entries_ld_.valid) {
    return entries_ld_;
  }
  if (entries_ex_.valid) {
    return entries_ex_;
  }
  if (entries_st_.valid) {
    return entries_st_;
  }
  return entries_ex_;
}

// read load RS row
const SmeshRsEntry& SmeshRS::loadEntry() const {
  return entries_ld_;
}
// read execute RS row
const SmeshRsEntry& SmeshRS::executeEntry() const {
  return entries_ex_;
}
// read store RS row
const SmeshRsEntry& SmeshRS::storeEntry() const {
  return entries_st_;
}

// issue load entry to load issue port
const SmeshRsEntry* SmeshRS::issueLoad() const {
  return (entries_ld_.valid && !entries_ld_.issued) ? &entries_ld_ : nullptr;
}
// issue execute entry to execute issue port
const SmeshRsEntry* SmeshRS::issueExecute() const {
  return (entries_ex_.valid && !entries_ex_.issued) ? &entries_ex_ : nullptr;
}
// issue store entry to store issue port
const SmeshRsEntry* SmeshRS::issueStore() const {
  return (entries_st_.valid && !entries_st_.issued) ? &entries_st_ : nullptr;
}

bool SmeshRS::markIssued(SmeshRobId rob_id) {
  for (auto* entry : {&entries_ld_, &entries_ex_, &entries_st_}) {
    if (entry->valid && entry->rob_id == rob_id) {
      entry->issued = true;
      return true;
    }
  }

  return false;
}

bool SmeshRS::complete(SmeshRobId rob_id) {
  for (auto* entry : {&entries_ld_, &entries_ex_, &entries_st_}) {
    if (entry->valid && entry->rob_id == rob_id) {
      *entry = SmeshRsEntry{};
      return true;
    }
  }

  return false;
}

} // namespace smesh
