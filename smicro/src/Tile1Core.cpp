// **********************************************************************
// smicro/src/Tile1Core.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Dec 2 2025
/*
Minimal wrapper to host Tile1 in smicro as a Component.  See Tile1Core.hpp for details.
*/

#include "Tile1Core.hpp"

using namespace Cascade;

// Define the nested DramMemoryPort adapter helper class
// Simple adapter/shim class that turns Tile1's read32/write32 into Dram::read/write
// i.e., exposes smicro's Dram as a MemoryPort for Tile1 (only viewable in this .cpp)
class Tile1Core::DramMemoryPort : public MemoryPort {
public:
  explicit DramMemoryPort(Dram& dram) : dram_(dram) {}

  uint32_t read32(uint32_t addr) override {
    uint32_t value = 0;
    const uint64_t phys = dram_.get_base() + static_cast<uint64_t>(addr);
    dram_.read(phys, &value, sizeof(value));
    return value;
  }

  void write32(uint32_t addr, uint32_t value) override {
    const uint64_t phys = dram_.get_base() + static_cast<uint64_t>(addr);
    dram_.write(phys, &value, sizeof(value));
  }
  
  void cycle() override {}
  
  bool can_request() const override {
    return !resp_valid_;
  }

  void request_read32(uint32_t addr) override {
    resp_data_ = read32(addr);
    resp_valid_ = true;
  }

  void request_write32(uint32_t addr, uint32_t value) override {
    write32(addr, value);
    resp_data_ = 0;
    resp_valid_ = true;
  }

  bool resp_valid() const override {
    return resp_valid_;
  }

  uint32_t resp_data() const override {
    return resp_data_;
  }

  void resp_consume() override {
    resp_valid_ = false;
  }

private:
  Dram& dram_;
  bool resp_valid_ = false;
  uint32_t resp_data_ = 0;
};


Tile1Core::Tile1Core(std::string name, IMPL_CTOR) // Tile1Core constructor implementation
  : tile_("tile1")  // Tile1 has a convenience ctor taking just a name
{
  tile_.clk << clk; // connect Tile1's clock to Tile1Core wrapper clock
}

void Tile1Core::attach_dram(Dram* dram) { // to tell Tile1Core which DRAM instance to use, 
  dram_ = dram;   // remembers which DRAM instance we're using

  // Clean up any existing adapter
  if (dram_port_) {
    delete dram_port_;
    dram_port_ = nullptr;
  }

  // If a valid DRAM is provided, create a new adapter and hook Tile1 to it
  if (dram_) {
    dram_port_ = new DramMemoryPort(*dram_); // re-uses DramMemoryPort defined above, so tick() sees a synchronous memory just like tb_tile1.cpp
    tile_.attach_memory(dram_port_);         // gives DramMemoryPort pointer to Tile1 so it can do memory accesses
  }
}

void Tile1Core::update() {
  tile_.tick();   // for now: just tick Tile1. Memory is handled synchronously via DramMemoryPort.
}

void Tile1Core::reset() {
  // You can add Tile1-specific reset if/when you have one.
}
