// **********************************************************************
// smicro/src/L1.hpp
// **********************************************************************
// S Magierowski Aug 16 2025
#pragma once
#include <cascade/Cascade.hpp>
#include "smem/MemTypes.hpp"

class L1 : public Component {
  DECLARE_COMPONENT(L1);
public:
  L1(std::string name, COMPONENT_CTOR);
  Clock(clk);

  // Upstream (core-facing)
  FifoInput (smem::MemReq,  up_req);
  FifoOutput(smem::MemResp, up_resp);
  // Downstream (L2-facing)
  FifoOutput(smem::MemReq,  down_req);
  FifoInput (smem::MemResp, down_resp);
  void update();
  void reset();
};
