// **********************************************************************
// smicro/include/SmicroTypes.hpp
// **********************************************************************
// S Magierowski Aug 16 2025

#pragma once

enum AttachMode { ViaL1, ViaL2, ToDRAM, PrivateDRAM };

inline const char* attachModeName(AttachMode m) {
  switch (m) {
    case ViaL1:       return "ViaL1";
    case ViaL2:       return "ViaL2";
    case ToDRAM:      return "ToDRAM";
    case PrivateDRAM: return "PrivateDRAM";
    default:          return "Unknown";
  }
}
