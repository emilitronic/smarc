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

struct SmeshOperandRange {
  bool valid = false;
  std::uint32_t start_row = 0;
  std::uint32_t rows = 0;
};

struct SmeshRsEntry {
  bool valid = false;
  bool issued = false;
  SmeshQueueClass queue = SmeshQueueClass::System;
  SmeshCmd cmd{};
  SmeshRobId rob_id = 0;

  // Reserved for later dependency tracking.
  SmeshOperandRange opa{};
  SmeshOperandRange opb{};
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

} // namespace smesh
