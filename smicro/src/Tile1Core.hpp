// **********************************************************************
// smicro/src/Tile1Core.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Dec 2 2025
/*
Wrapper for Tile1 that plugs into smicro ecosystem.

          (call graph / interfaces)

           Tile1 (CPU)
               |
               |  MemoryPort::read32/write32
               v
   DramMemoryPort (adapter, lives *inside* Tile1Core)
               |
               |  Dram::read/write (addr = base + PC)
               v
        Dram (smicro DRAM)

          Tile1Core (wrapper component)
          +---------------------------------+
          |  Tile1 tile_;                   |
          |  DramMemoryPort* dram_port_;    |
          |  Dram* dram_;                   |
          +---------------------------------+
*/
#pragma once
#include <cascade/Cascade.hpp>
#include "MemTypes.hpp"
#include "Tile1.hpp"        // tile from smile
#include "AccelPort.hpp"    // if you want custom-0 wired too
#include "Dram.hpp"         // if you want to connect DRAM

class Tile1Core : public Component {
  DECLARE_COMPONENT(Tile1Core);
public:
  Tile1Core(std::string name, COMPONENT_CTOR);
  // -----------------------------
  // Interface ports
  // -----------------------------
  Clock(clk);
  FifoOutput(MemReq,  m_req);   // same interface as RvCore
  FifoInput (MemResp, m_resp);
  
  // -----------------------------
  // Simulation methods
  // -----------------------------
  void update();
  void reset();
  void attach_dram(Dram* dram); // let SoC give Tile1Core a DRAM to talk to

private:
  Tile1 tile_;                  // the actual RISC-V core
  Dram* dram_ = nullptr;        // the DRAM to connect to
  // We'll allocate this in the .cpp once we know the Dram
  class DramMemoryPort;         // forward declaration of nested adapter helper class
  DramMemoryPort* dram_port_ = nullptr;
};