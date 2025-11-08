// **********************************************************************
// smile/src/util/FlatBinLoader.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 15 2025

#pragma once

#include <cstdint>
#include <string>

class MemoryPort; // forward decl (Tile1.hpp defines it)

/*
Load a flat little-endian binary into memory via MemoryPort::write32().
- Pads the tail with zeros to a full 32-bit word (so no byte writes needed).
- Returns true on success; optionally writes the number of file bytes loaded.
*/
bool load_flat_bin(const std::string& path, MemoryPort* mem, uint32_t base_addr, uint32_t* bytes_loaded_out = nullptr);