// **********************************************************************
// smicro/src/Tile1Core.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Dec 2 2025
/*
Minimal wrapper to host Tile1 in smicro as a Component.  See Tile1Core.hpp for details.
*/

#include "Tile1Core.hpp"
#include "AccelPort.hpp"
#include "smem/DramMemoryPort.hpp"

using namespace Cascade;

Tile1Core::Tile1Core(std::string name, IMPL_CTOR) // Tile1Core constructor implementation
  : tile_("tile1")  // Tile1 has convenience ctor taking just a name
{
  tile_.clk << clk; // connect Tile1's clock to Tile1Core wrapper clock
}

void Tile1Core::attach_dram(smem::Dram* dram) { // to tell Tile1Core which DRAM instance to use, 
  dram_ = dram; // remembers which DRAM instance we're using (i.e., save the pointer)

  // Clean up any existing adapter
  if (dram_port_) {
    delete dram_port_;
    dram_port_ = nullptr;
  }

  // If a valid DRAM is provided, create a new adapter and hook Tile1 to it
  if (dram_) {
    dram_port_ = new smem::DramMemoryPort(*dram_); // shared adapter from smem
    tile_.attach_memory(dram_port_);         // gives DramMemoryPort pointer to Tile1 so it can do memory accesses
  }
}

void Tile1Core::attach_accelerator(AccelPort* accel) {
  tile_.attach_accelerator(accel);
}

void Tile1Core::set_pc(uint32_t pc) {
  tile_.set_pc(pc);
}

void Tile1Core::update() {
  tile_.tick();   // for now: just tick Tile1. Memory is handled synchronously via DramMemoryPort.
}

void Tile1Core::reset() {
  // You can add Tile1-specific reset if/when you have one.
}
