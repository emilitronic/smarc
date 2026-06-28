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
};
// RS's op* sub-section
struct SmeshRSOp {
  bool valid = false;   // are op* contents valid?
  SmeshRSOpBits bits{};
};

// RS row sections
struct SmeshRsEntry {
  bool valid = false;
  bool issued = false;
  SmeshQueueClass queue = SmeshQueueClass::System;
  SmeshCmd cmd{};
  SmeshRobId rob_id = 0;

  // Reserved for later dependency tracking.
  SmeshRSOp opa{};
  SmeshRSOp opb{};
  bool opa_is_dst = false;
  std::uint32_t deps = 0;
};

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
    }

    case SmeshFunct::Flush:
      return SmeshQueueClass::System;
  }

  return SmeshQueueClass::Invalid;
}

// RS row owner and manager
class SmeshRS {
public:
  bool empty() const;
  bool busy() const;
  bool canAccept() const;

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
};

} // namespace smesh
