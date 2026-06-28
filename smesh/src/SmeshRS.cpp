// **********************************************************************
// smesh/src/SmeshRS.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jun 24 2026
/*
One-entry reservation-station state machine for smesh.
*/

#include "SmeshRS.hpp"

#include <limits>
#include <stdexcept>

namespace smesh {
namespace {

std::uint32_t localAddrRaw(std::uint64_t packed) {
  return static_cast<std::uint32_t>(packed & kLocalAddrMask);
}
// set op* fields based on local_addr and rows_touched for any command
SmeshRSOp makeRSOp(std::uint64_t packed, std::uint32_t rows_touched) {
  const auto start = makeLocalAddr(localAddrRaw(packed));  // extract local_addr from packed rs1/rs2

  SmeshRSOp op{};
  op.valid = true;
  op.bits.start = start;
  op.bits.end = addLocalAddr(start, rows_touched);
  op.bits.wraps_around = addLocalAddrOverflows(start, rows_touched);
  return op;
}

SmeshRSOp makeRSOp(std::uint64_t packed) {
  return makeRSOp(packed, static_cast<std::uint32_t>(unpackLocal(packed).shape.rows));
}
// compute bounding-range of rows touched by LOAD
std::uint32_t loadRowsTouched(MatrixShape shape, std::uint32_t block_stride) {
  if (shape.rows == 0 || shape.cols == 0) {
    return 0;
  }

  const std::uint64_t chunks = ((shape.cols - 1) / kDim) + 1; // ceil(num_cols / DIM)
  const std::uint64_t extent = (chunks - 1) * block_stride + static_cast<std::uint64_t>(shape.rows); // (chunks - 1) * block_stride + num_rows
  if (extent > std::numeric_limits<std::uint32_t>::max()) {
    throw std::overflow_error("LOAD local-memory range is too large");
  }
  return static_cast<std::uint32_t>(extent);
}
// compute bounding-range of rows touched by STORE
std::uint32_t storeRowsTouched(MatrixShape shape) {
  if (shape.rows == 0 || shape.cols == 0) {
    return 0;
  }

  const std::uint64_t chunks = ((shape.cols - 1) / kDim) + 1; // ceil(num_cols / DIM)
  const std::uint64_t extent = (chunks - 1) * kDim + static_cast<std::uint64_t>(shape.rows); // (chunks - 1) * DIM + num_rows
  if (extent > std::numeric_limits<std::uint32_t>::max()) {
    throw std::overflow_error("STORE local-memory range is too large");
  }
  return static_cast<std::uint32_t>(extent);
}
// find which LOAD it is, 1, 2, or 3
std::size_t loadStateId(SmeshFunct funct) {
  if (funct == SmeshFunct::Mvin2) {
    return 1;
  }
  if (funct == SmeshFunct::Mvin3) {
    return 2;
  }
  return 0;
}
// watches for CONFIG commands and updates the RS's config_state_ accordingly
void updateConfigState(const SmeshCmd& cmd, SmeshRSConfigState& state) {
  const auto funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(cmd.funct));
  if (funct != SmeshFunct::Config) {
    return;
  }

  const auto rs1 = static_cast<std::uint64_t>(cmd.rs1);
  const auto kind = static_cast<ConfigKind>(rs1 & 0x3u);
  if (kind != ConfigKind::Load) {
    return;
  }

  const auto state_id = unpackConfigStateId(rs1);
  if (state_id < state.ld_block_stride.size()) {
    state.ld_block_stride.at(state_id) = unpackConfigLoadBlockStride(rs1);
  }
}

// fill RS entry's op* sections on allocate (a dispatcher)
void fillOperands(SmeshRsEntry& entry, const SmeshRSConfigState& config_state) {
  const auto funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(entry.cmd.funct));

  entry.opa = {};
  entry.opb = {};
  entry.opa_is_dst = false;
  // map command arguments to appropriate RS op* sections
  switch (funct) {
    case SmeshFunct::Mvin:
    case SmeshFunct::Mvin2:
    case SmeshFunct::Mvin3: {
      const auto packed = static_cast<std::uint64_t>(entry.cmd.rs2); // read shape & local_addr from rs2
      const auto state_id = loadStateId(funct);  // is it LOAD,LOAD2, or LOAD3?
      const auto rows_touched = loadRowsTouched(unpackLocal(packed).shape, config_state.ld_block_stride.at(state_id)); // strip extent
      entry.opa = makeRSOp(packed, rows_touched);  // make opa for LOAD
      entry.opa_is_dst = true;
      break;
    }

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

    case SmeshFunct::Mvout: {
      const auto packed = static_cast<std::uint64_t>(entry.cmd.rs2);
      const auto rows_touched = storeRowsTouched(unpackLocal(packed).shape);
      entry.opa = makeRSOp(packed, rows_touched);
      entry.opa_is_dst = false;
      break;
    }

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
  updateConfigState(cmd, config_state_);
  fillOperands(*slot, config_state_); // entry's op* sections

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

// read LOAD RS row
const SmeshRsEntry& SmeshRS::loadEntry() const {
  return entries_ld_;
}
// read EXECUTE RS row
const SmeshRsEntry& SmeshRS::executeEntry() const {
  return entries_ex_;
}
// read STORE RS row
const SmeshRsEntry& SmeshRS::storeEntry() const {
  return entries_st_;
}

// issue LOAD entry to LOAD issue port
const SmeshRsEntry* SmeshRS::issueLoad() const {
  return (entries_ld_.valid && !entries_ld_.issued) ? &entries_ld_ : nullptr;
}
// issue EXECUTE entry to EXECUTE issue port
const SmeshRsEntry* SmeshRS::issueExecute() const {
  return (entries_ex_.valid && !entries_ex_.issued) ? &entries_ex_ : nullptr;
}
// issue STORE entry to STORE issue port
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
