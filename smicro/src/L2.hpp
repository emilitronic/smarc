// smicro/src/L2.hpp
#pragma once
#include <cascade/Cascade.hpp>
#include "smem/MemTypes.hpp"

class L2 : public Component {
  DECLARE_COMPONENT(L2);
public:
  L2(std::string name, COMPONENT_CTOR);
  Clock(clk);

  // Core-facing
  FifoInput (smem::MemReq,  core_req);
  FifoOutput(smem::MemResp, core_resp);
  // DRAM-facing
  FifoOutput(smem::MemReq,  mem_req);
  FifoInput (smem::MemResp, mem_resp);
  // Accelerator-facing
  FifoInput (smem::MemReq,  accel_req);
  FifoOutput(smem::MemResp, accel_resp);
  void update();
  void reset();
};
