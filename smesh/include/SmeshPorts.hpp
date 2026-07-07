// **********************************************************************
// smesh/include/SmeshPorts.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026
/*
A collection of ports between smesh components.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshLocalAddr.hpp"

#include <cstdint>

namespace smesh {

// ********** DRIVER / SHELL INTERFACE **********
// interface between SmeshCommandDriver and SmeshShell
struct SmeshCmd {
  u32 funct = 0;
  u64 rs1 = 0;
  u64 rs2 = 0;
};

struct SmeshResp {
  u8 status = 0;
  u64 value = 0;
};

// ********** RS / CONTROLLER INTERFACE **********
// interface between RS and Ld/St/Ex controllers
using SmeshRobId = std::uint16_t;

struct SmeshIssue {
  SmeshCmd cmd{};
  SmeshRobId rob_id = 0;
};

// ********** LOAD CONTROLLER / DMA INTERFACE **********

struct DmaReadReq {
  u64 vaddr = 0;
  SmeshLocalAddr laddr{};
  u16 cols = 0;
  u16 repeats = 0;
  u32 scale = 0;
  bit has_acc_bitwidth = false;
  bit all_zeros = false;
  u16 block_stride = 0;
  u8 pixel_repeats = 1;
  u16 cmd_id = 0;
};

struct DmaReadResp {
  u64 data = 0;
  SmeshLocalAddr laddr{};
  u8 mask = 0;
  u16 bytes_read = 0;
  u8 pixel_repeats = 1;
  u16 cmd_id = 0;
  bit last = false;
};

} // namespace smesh
