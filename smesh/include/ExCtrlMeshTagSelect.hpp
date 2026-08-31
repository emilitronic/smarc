// **********************************************************************
// smesh/include/ExCtrlMeshTagSelect.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 16 2026
/*
Combinational selector for the RS tag carried in mesh-control packets.

If current operation is not a standalone mult and the C destination is not garbage, 
then the RS tag is valid and should be used to generate a completion packet.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "ExCtrlQueues.hpp"
#include "SmeshLocalAddr.hpp"
#include "SmeshPorts.hpp"

namespace smesh {

class ExCtrlMeshTagSelect : public Component {
  DECLARE_COMPONENT(ExCtrlMeshTagSelect);

 public:
  ExCtrlMeshTagSelect(std::string name, COMPONENT_CTOR);

  Clock(clk);

  InputArray(bit,        head_val,  kExCtrlCmdWindow); // visible command-window valid bits
  InputArray(SmeshIssue, head_bits, kExCtrlCmdWindow); // visible command-window payloads
  Input(u8,              preload_cmd_place);           // which cmd slot has PRELOAD, 0 or 1
  Input(bit,             performing_single_mul);       // single-mul packets do not carry completion tags
  Input(SmeshLocalAddr,  c_address_rs2);               // garbage C suppresses mesh completion tag

  Output(bit,        mesh_rs_tag_valid); // if true, an RS completion should be generated using this tag
  Output(SmeshRsTag, mesh_rs_tag);       // command's rs tag

  void update();
};

} // namespace smesh
