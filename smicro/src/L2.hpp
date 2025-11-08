// smicro/src/L2.hpp
#pragma once
#include <cascade/Cascade.hpp>
#include "MemTypes.hpp"

class L2 : public Component {
  DECLARE_COMPONENT(L2);
public:
  L2(std::string name, COMPONENT_CTOR);
  Clock(clk);

  // Core-facing
  FifoInput (MemReq,  core_req);
  FifoOutput(MemResp, core_resp);
  // DRAM-facing
  FifoOutput(MemReq,  mem_req);
  FifoInput (MemResp, mem_resp);
  // Accelerator-facing
  FifoInput (MemReq,  accel_req);
  FifoOutput(MemResp, accel_resp);
  void update();
  void reset();
};
