// **********************************************************************
// smicro/include/Utils.hpp
// **********************************************************************
// S Magierowski Aug 16 2025

#pragma once
#include <cstdint>
#include <string>

namespace utils {
inline int to_int(const std::string& s, int def = 0) {
  try { return std::stoi(s); } catch (...) { return def; }
}
}

