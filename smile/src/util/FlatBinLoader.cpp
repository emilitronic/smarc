// **********************************************************************
// smile/src/util/FlatBinLoader.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 15 2025 

#include "FlatBinLoader.hpp"
#include "Tile1.hpp" // for MemoryPort
#include <cstdint>
#include <fstream>
#include <vector>

namespace {
inline void write32(MemoryPort* mem, uint32_t addr, uint32_t word) {
  mem->write32(addr, word);
}
} // namespace

bool load_flat_bin(const std::string& path, MemoryPort* mem, uint32_t base_addr, uint32_t* bytes_loaded_out) {
  std::ifstream f(path, std::ios::binary); // open file in binary mode
  if (!f) return false;

  std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); // stream iterators "slurp" file into buf[]
  if (buf.empty()) return false; // buf[i] contains ith byte

  uint32_t addr = base_addr;
  size_t i = 0;

  // Pack 4B chunks into one 32b littl-endian word (lowest byte in LSB), then writes to mem
  for (; i + 4 <= buf.size(); i += 4, addr += 4) {
    uint32_t w =  (uint32_t)buf[i]
                | ((uint32_t)buf[i + 1] << 8)
                | ((uint32_t)buf[i + 2] << 16)
                | ((uint32_t)buf[i + 3] << 24);
    write32(mem, addr, w); 
  }
  
  // Handle leftover bytes. If total byte can't wasn't multiple of 4 there are 1-3 bytes left.
  if (i < buf.size()) {
    uint32_t w = 0;
    int shift = 0;
    for (; i < buf.size(); ++i, shift += 8) {
      w |= ((uint32_t)buf[i] << shift);
    }
    write32(mem, addr, w);
    addr += 4;
  }
  
  // If caller passed pointer for bytes_loaded_out, writes how many bytes were loaded.
  if (bytes_loaded_out) *bytes_loaded_out = (uint32_t)buf.size();
  return true;
}
