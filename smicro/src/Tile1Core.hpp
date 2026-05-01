// **********************************************************************
// smicro/src/Tile1Core.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Dec 2 2025
/*
Tile1Core: minimal wrapper to host Tile1 inside smicro ecosystem.

+--------------------------Tile1Core-------------------------+
|          tile_                         dram_port_          |             dram_
| +--------Tile1---------+      +------DramMemoryPort------+ |    +--------Dram--------+
| | mem_port_->read32()  |<-----| read32(addr)->data  read |<-----| read(phys,&dst,n)  |
| | mem_port_->write32() |----->| write32(addr,data)  write|----->| write(phys,&src,n) |
| +----------------------+      +--------------------------+ |    +--------------------+  
|                 MemoryPort::read32/write32                 | Dram::read/write
+------------------------------------------------------------+
*/
#pragma once
#include <cascade/Cascade.hpp>
#include <cstdint>
#include "smem/MemTypes.hpp"
#include "Tile1.hpp"        // tile from smile
#include "smem/Dram.hpp"         // if you want to connect DRAM

class AccelPort;
namespace smem { class DramMemoryPort; }

class Tile1Core : public Component {
  DECLARE_COMPONENT(Tile1Core);

public:
  Tile1Core(std::string name, COMPONENT_CTOR);

  // -----------------------------
  // Interface ports
  // -----------------------------
  Clock(clk);
  FifoOutput(smem::MemReq,  m_req);   // same interface as RvCore
  FifoInput (smem::MemResp, m_resp);
  
  // -----------------------------
  // Simulation methods
  // -----------------------------
  void update();
  void reset();
  void attach_dram(smem::Dram* dram); // let SoC give Tile1Core a DRAM to talk to
  void attach_accelerator(AccelPort* accel);
  void set_pc(uint32_t pc);

private:
  Tile1 tile_;                  // the actual RISC-V core (in smile)
  smem::Dram* dram_ = nullptr;  // the DRAM to connect to
  // Shared adapter from smem, allocated once DRAM is attached.
  smem::DramMemoryPort* dram_port_ = nullptr;
};
