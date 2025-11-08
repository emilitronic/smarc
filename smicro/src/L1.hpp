// **********************************************************************
// smicro/src/L1.hpp
// **********************************************************************
// S Magierowski Aug 16 2025
#pragma once
#include <cascade/Cascade.hpp>
#include "MemTypes.hpp"

class L1 : public Component {
  DECLARE_COMPONENT(L1);
public:
  L1(std::string name, COMPONENT_CTOR);
  Clock(clk);

  // Upstream (core-facing)
  FifoInput (MemReq,  up_req);
  FifoOutput(MemResp, up_resp);
  // Downstream (L2-facing)
  FifoOutput(MemReq,  down_req);
  FifoInput (MemResp, down_resp);
  void update();
  void reset();
};
