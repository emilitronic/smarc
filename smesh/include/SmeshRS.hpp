// **********************************************************************
// smesh/include/SmeshRS.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jun 24 2026
/*
Reservation-station vocabulary for smesh.

This header defines the lightweight entry and classification types. The Cascade
reservation-station component will be introduced separately.
*/
#pragma once

#include "SmeshCommand.hpp"
#include "SmeshPorts.hpp"

#include <array>
#include <cstdint>

namespace smesh {

enum class SmeshQueueClass : std::uint8_t {
  Load,
  Execute,
  Store,
  System,
  Invalid,
};

using SmeshRobId = std::uint16_t;

// Runtime state programmed by CONFIG commands and used to calculate RS
// local-memory ranges. These strides are measured in local rows.
struct SmeshRSConfigState {
  std::array<std::uint32_t, kLoadStates> ld_block_stride{};
  std::uint32_t a_stride = 0;
  std::uint32_t c_stride = 0;
  bool a_transpose = false;
};

// RS's op* section's bit fields
struct SmeshRSOpBits {
  SmeshLocalAddr start{};    // 32b
  SmeshLocalAddr end{};      // 32b
  bool wraps_around = false; //  1b

  constexpr bool overlaps(const SmeshRSOpBits& other) const {
    // reject ranges that can't overlap
    // garbage can't overlap, spad range can't overlap acc range
    if (start.is_garbage() || other.start.is_garbage() ||
        start.is_acc_addr() != other.start.is_acc_addr()) {
      return false;
    }
    // convert to flattened addresses for comparison, ignoring metadata
    const auto this_start  = start.is_acc_addr() ? start.full_acc_addr() : start.full_sp_addr();
    const auto this_end    = end.is_acc_addr() ? end.full_acc_addr() : end.full_sp_addr();
    const auto other_start = other.start.is_acc_addr() ? other.start.full_acc_addr() : other.start.full_sp_addr();
    const auto other_end   = other.end.is_acc_addr() ? other.end.full_acc_addr() : other.end.full_sp_addr();
    
    if (!wraps_around && this_start == this_end) { // non-wrapping half-open range, e.g., [4,4) has no rows
      return false;
    } 
    if (!other.wraps_around && other_start == other_end) { // non-wrapping half-open range
      return false;
    }
    if (wraps_around && other.wraps_around) { // two non-empty wrapping ranges must overlap (they *both* cross mem boundary)
      return true;
    }
    if (wraps_around) {
      return other_start < this_end || this_start < other_end; // if this wraps, does other overlap either piece
    }
    if (other.wraps_around) {
      return this_start < other_end || other_start < this_end; // if other wraps, does this overlap either piece
    }
    return this_start < other_end && other_start < this_end; // neither wraps, [a,b) overlaps [c,d) iff a < d && c < b]
  }
};
// RS's op* sub-section
struct SmeshRSOp {
  bool valid = false;   // are op* contents valid?
  SmeshRSOpBits bits{};
};

// *** RS row contents ***
// ***********************
struct SmeshRsEntry {
  bool valid = false;      // smesh keeps the outer entry-valid bit here
  SmeshQueueClass q = SmeshQueueClass::System;
  bool is_config = false;  // is it a CONFIG cmd?

  SmeshRSOp opa{};
  bool opa_is_dst = false; //
  SmeshRSOp opb{};

  bool issued = false;
  bool complete_on_issue = false; // TODO: use this when issue/free timing is modeled

  SmeshCmd cmd{};
  SmeshRobId rob_id = 0; // smesh v0 ID; Gemmini derives this from queue and row

  std::uint32_t deps_ld = 0; // bit 0 represents the current load entry
  std::uint32_t deps_ex = 0; // bit 0 represents the current execute entry
  std::uint32_t deps_st = 0; // bit 0 represents the current store entry
  std::uint32_t allocated_at = 0; // allocation sequence number for debugging
  // convenience fn: all dependencies cleared for this entry?
  bool ready() const {
    return deps_ld == 0 && deps_ex == 0 && deps_st == 0;
  }
};
// decide which RS row should receive a command
inline SmeshQueueClass classifyCommand(const SmeshCmd& cmd) {
  const auto funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(cmd.funct));

  switch (funct) {
    case SmeshFunct::Mvin:
    case SmeshFunct::Mvin2:
    case SmeshFunct::Mvin3:
      return SmeshQueueClass::Load;

    case SmeshFunct::Preload:
    case SmeshFunct::ComputeFlip:
    case SmeshFunct::ComputeStay:
      return SmeshQueueClass::Execute;

    case SmeshFunct::Mvout:
    case SmeshFunct::StoreSpad:
      return SmeshQueueClass::Store;

    case SmeshFunct::Config: {
      const auto kind = static_cast<ConfigKind>(static_cast<std::uint64_t>(cmd.rs1) & 0x3u);
      switch (kind) {
        case ConfigKind::Load:
          return SmeshQueueClass::Load;
        case ConfigKind::Execute:
          return SmeshQueueClass::Execute;
        case ConfigKind::Store:
          return SmeshQueueClass::Store;
      }
      return SmeshQueueClass::Invalid;
    } // config cmd has sub-types, so classify by kind

    case SmeshFunct::Flush:
      return SmeshQueueClass::System; // system cmd bypasses RS
  }

  return SmeshQueueClass::Invalid; // cmd can't be classified
}

// RS row owner and manager
class SmeshRS {
public:
  bool empty() const;
  bool busy() const;
  bool canAccept(const SmeshCmd& cmd) const;

  const SmeshRSConfigState& configState() const;

  bool allocate(const SmeshCmd& cmd); // accept new cmd into RS slot
  bool allocate(const SmeshCmd& cmd, SmeshRobId* rob_id_out);
  const SmeshRsEntry& entry() const;
  const SmeshRsEntry& loadEntry() const;
  const SmeshRsEntry& executeEntry() const;
  const SmeshRsEntry& storeEntry() const;

  const SmeshRsEntry* issueLoad() const;
  const SmeshRsEntry* issueExecute() const;
  const SmeshRsEntry* issueStore() const;

  bool markIssued(SmeshRobId rob_id);
  bool complete(SmeshRobId rob_id);

private:
  SmeshRSConfigState config_state_{};
  SmeshRsEntry entries_ld_{};
  SmeshRsEntry entries_ex_{};
  SmeshRsEntry entries_st_{};
  SmeshRobId next_rob_id_ = 0;
  std::uint32_t instructions_allocated_ = 0;
};

} // namespace smesh
