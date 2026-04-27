// **********************************************************************
// smesh/include/SmeshMemory.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Apr 26 2026
/*
Tiny fake host memory.
*/
#pragma once

#include "SmeshTypes.hpp"

#include <cstdint>
#include <map>

namespace smesh {

class SmeshMemory {
 public:
  void reset() { bytes_.clear(); }

  void writeElem(std::uint64_t addr, Elem value) {
    bytes_[addr] = static_cast<std::uint8_t>(value);
  }

  Elem readElem(std::uint64_t addr) const {
    return static_cast<Elem>(readByte(addr));
  }

  void writeAcc(std::uint64_t addr, Acc value) {
    const auto uvalue = static_cast<std::uint32_t>(value);
    for (std::uint64_t i = 0; i < sizeof(Acc); ++i) {
      bytes_[addr + i] = static_cast<std::uint8_t>((uvalue >> (8 * i)) & 0xffu);
    }
  }

  Acc readAcc(std::uint64_t addr) const {
    std::uint32_t value = 0;
    for (std::uint64_t i = 0; i < sizeof(Acc); ++i) {
      value |= static_cast<std::uint32_t>(readByte(addr + i)) << (8 * i);
    }
    return static_cast<Acc>(value);
  }

 private:
  std::uint8_t readByte(std::uint64_t addr) const {
    const auto it = bytes_.find(addr);
    return it == bytes_.end() ? 0 : it->second;
  }

  std::map<std::uint64_t, std::uint8_t> bytes_;
};

} // namespace smesh
