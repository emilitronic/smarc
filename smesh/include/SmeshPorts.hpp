// **********************************************************************
// smesh/include/SmeshPorts.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026
/*
FIFO packet structs for talking to the SmeshShell and SmeshCommandDriver components.  
These are just simple structs that define the data format for commands and responses 
sent between the driver and the shell.  The actual command encoding/decoding logic 
is in SmeshDevice::executeCustom() and the testbench scripts.
*/
#pragma once

#include <cascade/Cascade.hpp>

namespace smesh {

struct SmeshCmd {
  u32 funct = 0;
  u64 rs1 = 0;
  u64 rs2 = 0;
};

struct SmeshResp {
  u8 status = 0;
  u64 value = 0;
};

} // namespace smesh
