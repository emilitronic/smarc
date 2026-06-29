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

// ********** OPERAND RANGE HELPERS **********

std::uint32_t localAddrRaw(std::uint64_t packed) {
  return static_cast<std::uint32_t>(packed & kLocalAddrMask);
}
// set op* fields based on local_addr and rows_touched for any command
SmeshRSOp makeRSOp(std::uint64_t packed, std::uint32_t rows_touched) {
  const auto start = makeLocalAddr(localAddrRaw(packed));  // extract local_addr from packed rs1/rs2

  SmeshRSOp op{};
  op.valid = true;
  op.bits.start = start;
  const auto end = add_with_overflow(start, rows_touched);
  op.bits.end = end.addr;
  op.bits.wraps_around = end.overflow;
  return op;
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
// compute bounding-range of rows touched by STORE and STORE_SPAD
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
// Current generated STORE_SPAD commands use one tile column and a
// destination stride of one, so their destination extent is num_rows
std::uint32_t storeSpadDstRowsTouched(MatrixShape shape) {
  return static_cast<std::uint32_t>(shape.rows);
}
// PRELOAD's B/D src occupies consecutive local rows
std::uint32_t preloadSrcRowsTouched(MatrixShape shape) {
  return static_cast<std::uint32_t>(shape.rows);
}
// PRELOAD's C dst uses configured execute C stride (set c_stride = 1 for consec row packing in C)
std::uint32_t preloadDstRowsTouched(MatrixShape shape, std::uint32_t c_stride) {
  const std::uint64_t extent = static_cast<std::uint64_t>(shape.rows) * c_stride;
  if (extent > std::numeric_limits<std::uint32_t>::max()) {
    throw std::overflow_error("PRELOAD destination range is too large");
  }
  return static_cast<std::uint32_t>(extent);
}
// COMPUTE's A src uses rows, or columns when A is transposed, and a_stride
std::uint32_t computeASrcRowsTouched(MatrixShape shape, std::uint32_t a_stride, bool a_transpose) {
  const auto stepped_rows = a_transpose ? shape.cols : shape.rows;
  const std::uint64_t extent = static_cast<std::uint64_t>(stepped_rows) * a_stride;
  if (extent > std::numeric_limits<std::uint32_t>::max()) {
    throw std::overflow_error("COMPUTE A source range is too large");
  }
  return static_cast<std::uint32_t>(extent);
}
// COMPUTE's B/D src occupies consecutive local rows
std::uint32_t computeBDSrcRowsTouched(MatrixShape shape) {
  return static_cast<std::uint32_t>(shape.rows);
}

// ********** CONFIGURATION HELPERS **********

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
  if (kind == ConfigKind::Execute) {
    state.a_stride = unpackConfigExecuteAStride(rs1);
    state.c_stride =
        unpackConfigExecuteCStride(static_cast<std::uint64_t>(cmd.rs2));
    state.a_transpose = unpackConfigExecuteATranspose(rs1);
    return;
  }
  if (kind != ConfigKind::Load) {
    return;
  }

  const auto state_id = unpackConfigStateId(rs1);
  if (state_id < state.ld_block_stride.size()) {
    state.ld_block_stride.at(state_id) = unpackConfigLoadBlockStride(rs1);
  }
}

// ********** OPERAND POPULATION **********

// fill RS entry's op* sections on allocate (a dispatcher)
// *******************************************************
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

    case SmeshFunct::Preload: {
      const auto source = static_cast<std::uint64_t>(entry.cmd.rs1);
      const auto destination = static_cast<std::uint64_t>(entry.cmd.rs2);
      entry.opa = makeRSOp(destination,preloadDstRowsTouched(unpackLocal(destination).shape, config_state.c_stride));
      entry.opb = makeRSOp(source, preloadSrcRowsTouched(unpackLocal(source).shape));
      entry.opa_is_dst = true;
      break;
    }

    case SmeshFunct::ComputeFlip:
    case SmeshFunct::ComputeStay: {
      const auto a_source = static_cast<std::uint64_t>(entry.cmd.rs1);
      const auto bd_source = static_cast<std::uint64_t>(entry.cmd.rs2);
      entry.opa = makeRSOp(
          a_source,
          computeASrcRowsTouched(unpackLocal(a_source).shape,
                                 config_state.a_stride,
                                 config_state.a_transpose));
      entry.opb = makeRSOp(
          bd_source,
          computeBDSrcRowsTouched(unpackLocal(bd_source).shape));
      entry.opa_is_dst = false;
      break;
    }

    case SmeshFunct::Mvout: {
      const auto packed = static_cast<std::uint64_t>(entry.cmd.rs2);
      const auto rows_touched = storeRowsTouched(unpackLocal(packed).shape);
      entry.opa = makeRSOp(packed, rows_touched);
      entry.opa_is_dst = false;
      break;
    }
    // for now keep STORE_SPAD dst stride of 1, so its destination extent is num_rows
    case SmeshFunct::StoreSpad: {
      const auto destination = static_cast<std::uint64_t>(entry.cmd.rs1); // packed dst addr (spad) and dst stride
      const auto source = static_cast<std::uint64_t>(entry.cmd.rs2);      // packed srd addr (spad or accum) and src shape
      const auto shape = unpackLocal(source).shape;                       // unpack src rows and cols from rs2
      entry.opa = makeRSOp(destination, storeSpadDstRowsTouched(shape)); // make opa
      entry.opb = makeRSOp(source, storeRowsTouched(shape));                     // make opb
      entry.opa_is_dst = true;
      break;
    }

    case SmeshFunct::Config:
    case SmeshFunct::Flush:
      break;
  }
}

// ********** DEPENDENCY HELPERS **********

// wrapper around overlaps() that checks for valid ops first
bool opOverlaps(const SmeshRSOp& lhs, const SmeshRSOp& rhs) {
  return lhs.valid && rhs.valid && lhs.bits.overlaps(rhs.bits);
}

// Return true when the new entry has a RAW, WAR, or WAW hazard with an older
// entry. opa is the destination when opa_is_dst is set; opb is always a source.
bool dependsOn(const SmeshRsEntry& new_entry, const SmeshRsEntry& older_entry) {
  if (!older_entry.valid) {
    return false;
  }

  // Same-queue commands remain ordered without checking operand ranges.
  if (new_entry.q == older_entry.q) {
    return new_entry.q == SmeshQueueClass::Store || !older_entry.issued;
  }
  if (new_entry.is_config) {
    return false;
  }

  // New write after an older read or write: WAR or WAW.
  if (new_entry.opa_is_dst) {
    if (opOverlaps(new_entry.opa, older_entry.opa) || opOverlaps(new_entry.opa, older_entry.opb)) {
      return true;
    }
  // New opa read after an older write: RAW.
  } else if (older_entry.opa_is_dst && opOverlaps(new_entry.opa, older_entry.opa)) {
    return true;
  }
  // New opb read after an older write: RAW.
  return older_entry.opa_is_dst && opOverlaps(new_entry.opb, older_entry.opa);
}
// Compute the dependency masks for a new entry based on all older entries in the RS
void fillDependencies(
    SmeshRsEntry& entry,
    const std::array<SmeshRsEntry, kDefaultConfig.rs_load_entries>& older_ld,
    const std::array<SmeshRsEntry, kDefaultConfig.rs_execute_entries>& older_ex,
    const std::array<SmeshRsEntry, kDefaultConfig.rs_store_entries>& older_st) {
  // clear all dependency masks
  entry.deps_ld = 0;
  entry.deps_ex = 0;
  entry.deps_st = 0;
  // check each older entry for a dependency and set the appropriate bit in the mask
  // dependency bit i corresponds to RS rows i
  for (std::size_t i = 0; i < older_ld.size(); ++i) {
    if (dependsOn(entry, older_ld[i])) { entry.deps_ld |= std::uint32_t{1} << i; }
  }
  for (std::size_t i = 0; i < older_ex.size(); ++i) {
    if (dependsOn(entry, older_ex[i])) { entry.deps_ex |= std::uint32_t{1} << i; }
  }
  for (std::size_t i = 0; i < older_st.size(); ++i) {
    if (dependsOn(entry, older_st[i])) { entry.deps_st |= std::uint32_t{1} << i; }
  }
}

} // namespace

// ********** RS STATUS **********

const SmeshRSConfigState& SmeshRS::configState() const {
  return config_state_;
}

bool SmeshRS::empty() const {
  return !entries_ld_[0].valid && !entries_ex_[0].valid && !entries_st_[0].valid;
}

bool SmeshRS::busy() const {
  return !empty();
}

// ********** ALLOCATION **********

// TODO: Share free-row selection with allocate() instead of searching twice.
// How does HW do it?
bool SmeshRS::canAccept(const SmeshCmd& cmd) const { // does a free row exist?
  switch (classifyCommand(cmd)) {
    case SmeshQueueClass::Load:
      for (const auto& entry : entries_ld_) {
        if (!entry.valid) {
          return true;
        }
      }
      return false;
    case SmeshQueueClass::Execute:
      for (const auto& entry : entries_ex_) {
        if (!entry.valid) {
          return true;
        }
      }
      return false;
    case SmeshQueueClass::Store:
      for (const auto& entry : entries_st_) {
        if (!entry.valid) {
          return true;
        }
      }
      return false;
    case SmeshQueueClass::System:
    case SmeshQueueClass::Invalid:
      return false;
  }
  return false;
}

bool SmeshRS::allocate(const SmeshCmd& cmd) { // convenience wrapper for allocate() that ignores rob_id_out
  return allocate(cmd, nullptr);
}
// places new command into appropriate RS entry and fills its operands and dependencies
bool SmeshRS::allocate(const SmeshCmd& cmd, SmeshRobId* rob_id_out) {
  if (!canAccept(cmd)) {
    return false;
  }

  SmeshRsEntry* slot = nullptr; // pointer to RS entry where new command will be placed
  const auto queue = classifyCommand(cmd);
  switch (queue) {
    case SmeshQueueClass::Load:
      for (auto& entry : entries_ld_) {
        if (!entry.valid) {
          slot = &entry;
          break;
        }
      }
      break;
    case SmeshQueueClass::Execute:
      for (auto& entry : entries_ex_) {
        if (!entry.valid) {
          slot = &entry;
          break;
        }
      }
      break;
    case SmeshQueueClass::Store:
      for (auto& entry : entries_st_) {
        if (!entry.valid) {
          slot = &entry;
          break;
        }
      }
      break;
    case SmeshQueueClass::System:
    case SmeshQueueClass::Invalid:
      return false;
  }
  if (slot == nullptr) {
    return false;
  }

  SmeshRsEntry new_entry{};
  new_entry.valid             = true;
  new_entry.q                 = queue;
  new_entry.is_config         = static_cast<SmeshFunct>(static_cast<std::uint32_t>(cmd.funct)) == SmeshFunct::Config;
  new_entry.issued            = false;
  new_entry.complete_on_issue = new_entry.is_config && queue != SmeshQueueClass::Execute; // true if config ld or st
  new_entry.cmd               = cmd;
  new_entry.rob_id            = next_rob_id_++;
  new_entry.allocated_at      = instructions_allocated_++;

  fillOperands(new_entry, config_state_);
  fillDependencies(new_entry, entries_ld_, entries_ex_, entries_st_);

  *slot = new_entry;
  updateConfigState(cmd, config_state_);

  if (rob_id_out != nullptr) {
    *rob_id_out = new_entry.rob_id;
  }
  return true;
}

// ********** ENTRY ACCESS **********

// which entry is currently occupied
const SmeshRsEntry& SmeshRS::entry() const {
  if (entries_ld_[0].valid) {
    return entries_ld_[0];
  }
  if (entries_ex_[0].valid) {
    return entries_ex_[0];
  }
  if (entries_st_[0].valid) {
    return entries_st_[0];
  }
  return entries_ex_[0];
}

// read LOAD RS row
const SmeshRsEntry& SmeshRS::loadEntry(std::size_t row) const {
  return entries_ld_.at(row);
}
// read EXECUTE RS row
const SmeshRsEntry& SmeshRS::executeEntry(std::size_t row) const {
  return entries_ex_.at(row);
}
// read STORE RS row
const SmeshRsEntry& SmeshRS::storeEntry(std::size_t row) const {
  return entries_st_.at(row);
}

// ********** ISSUE **********

// issue LOAD entry to LOAD issue port (search for oldest valid entry)
const SmeshRsEntry* SmeshRS::issueLoad() const {
  const SmeshRsEntry* oldest = nullptr;
  for (const auto& entry : entries_ld_) {
    // if row is valid, not issued, and ready (no dependencies), and is older than the current oldest, update oldest
    if (entry.valid && !entry.issued && entry.ready() &&
        (oldest == nullptr || entry.allocated_at < oldest->allocated_at)) {
      oldest = &entry;
    }
  }
  return oldest;
}
// issue EXECUTE entry to EXECUTE issue port (search for oldest valid entry)
const SmeshRsEntry* SmeshRS::issueExecute() const {
  const SmeshRsEntry* oldest = nullptr;
  for (const auto& entry : entries_ex_) {
    if (entry.valid && !entry.issued && entry.ready() &&
        (oldest == nullptr || entry.allocated_at < oldest->allocated_at)) {
      oldest = &entry;
    }
  }
  return oldest;
}
// issue STORE entry to STORE issue port (search for oldest valid entry)
const SmeshRsEntry* SmeshRS::issueStore() const {
  const SmeshRsEntry* oldest = nullptr;
  for (const auto& entry : entries_st_) {
    if (entry.valid && !entry.issued && entry.ready() &&
        (oldest == nullptr || entry.allocated_at < oldest->allocated_at)) {
      oldest = &entry;
    }
  }
  return oldest;
}

bool SmeshRS::markIssued(SmeshRobId rob_id) {
  for (auto* entry : {&entries_ld_[0], &entries_ex_[0], &entries_st_[0]}) {
    if (entry->valid && entry->rob_id == rob_id) {
      entry->issued = true;
      return true;
    }
  }

  return false;
}

// ********** COMPLETION **********

// mark RS entry as completed (based on rob_id)and free it, clearing dependencies in other entries
bool SmeshRS::complete(SmeshRobId rob_id) {
  // search all three entries
  for (auto* entry : {&entries_ld_[0], &entries_ex_[0], &entries_st_[0]}) {
    // find the valid entry that completed (rob_id matches)
    if (entry->valid && entry->rob_id == rob_id) {
      const auto completed_q = entry->q; // remember whether it was a LOAD, EXECUTE, or STORE entry
      // visit every entry that might have a dependency on the completed entry and clear that dependency
      for (auto* dependent : {&entries_ld_[0], &entries_ex_[0], &entries_st_[0]}) {
        switch (completed_q) {
          case SmeshQueueClass::Load:
            dependent->deps_ld &= ~std::uint32_t{1}; // clear bit 0 of deps_ld if LOAD completed
            break;
          case SmeshQueueClass::Execute:
            dependent->deps_ex &= ~std::uint32_t{1}; // clear bit 0 of deps_ex if EXECUTE completed
            break;
          case SmeshQueueClass::Store:
            dependent->deps_st &= ~std::uint32_t{1}; // clear bit 0 of deps_st if STORE completed
            break;
          case SmeshQueueClass::System:
          case SmeshQueueClass::Invalid:
            break;
        }
      }

      *entry = SmeshRsEntry{}; // clear completed entry itself
      return true;
    }
  }

  return false;
}

} // namespace smesh
