// **********************************************************************
// smesh/src/SmeshRS.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jun 24 2026
/*
One-entry reservation-station state machine for smesh.
*/

#include "SmeshRS.hpp"

namespace smesh {
namespace {

std::uint32_t localAddrRaw(std::uint64_t packed) {
  return static_cast<std::uint32_t>(packed & kLocalAddrMask);
}

SmeshRSOp makeRSOp(std::uint64_t packed) {
  const auto local = unpackLocal(packed);
  const auto start = makeLocalAddr(localAddrRaw(packed));
  const auto rows_touched = static_cast<std::uint32_t>(local.shape.rows);

  SmeshRSOp op{};
  op.valid = true;
  op.bits.start = start;
  op.bits.end = addLocalAddr(start, rows_touched);
  op.bits.wraps_around = addLocalAddrOverflows(start, rows_touched);
  return op;
}
// fill RS entry's op* sections on allocate
void fillOperands(SmeshRsEntry& entry) {
  const auto funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(entry.cmd.funct));

  entry.opa = {};
  entry.opb = {};
  entry.opa_is_dst = false;

  switch (funct) {
    case SmeshFunct::Mvin:
    case SmeshFunct::Mvin2:
    case SmeshFunct::Mvin3:
      entry.opa = makeRSOp(static_cast<std::uint64_t>(entry.cmd.rs2));
      entry.opa_is_dst = true;
      break;

    case SmeshFunct::Preload:
      entry.opa = makeRSOp(static_cast<std::uint64_t>(entry.cmd.rs2));
      entry.opb = makeRSOp(static_cast<std::uint64_t>(entry.cmd.rs1));
      entry.opa_is_dst = true;
      break;

    case SmeshFunct::ComputeFlip:
    case SmeshFunct::ComputeStay:
      entry.opa = makeRSOp(static_cast<std::uint64_t>(entry.cmd.rs1));
      entry.opb = makeRSOp(static_cast<std::uint64_t>(entry.cmd.rs2));
      entry.opa_is_dst = false;
      break;

    case SmeshFunct::Mvout:
      entry.opa = makeRSOp(static_cast<std::uint64_t>(entry.cmd.rs2));
      entry.opa_is_dst = false;
      break;

    case SmeshFunct::StoreSpad:
      entry.opa = makeRSOp(static_cast<std::uint64_t>(entry.cmd.rs1));
      entry.opb = makeRSOp(static_cast<std::uint64_t>(entry.cmd.rs2));
      entry.opa_is_dst = true;
      break;

    case SmeshFunct::Config:
    case SmeshFunct::Flush:
      break;
  }
}

} // namespace

const SmeshRSConfigState& SmeshRS::configState() const {
  return config_state_;
}

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
  slot->valid = true;              // entry's valid bit
  slot->issued = false;            // entry's issued bit
  slot->queue = queue;             // entry's queue type (load/execute/etc.)
  slot->cmd = cmd;                 // entry's command payload
  slot->rob_id = next_rob_id_++;
  fillOperands(*slot);             // entry's op* sections

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
