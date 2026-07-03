// **********************************************************************
// smesh/include/SmeshPorts.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026
/*
A collection of ports between smesh components.
*/
#pragma once

#include <cascade/Cascade.hpp>

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

} // namespace smesh
