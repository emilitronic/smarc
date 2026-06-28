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
// Current Gemmini-generated STORE_SPAD commands use one tile column and a
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
  slot->q = queue;                 // entry's queue type (load/execute/store)
  slot->is_config =
      static_cast<SmeshFunct>(static_cast<std::uint32_t>(cmd.funct)) ==
      SmeshFunct::Config;
  slot->issued = false;            // entry's issued bit
  slot->complete_on_issue = slot->is_config && queue != SmeshQueueClass::Execute;
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
