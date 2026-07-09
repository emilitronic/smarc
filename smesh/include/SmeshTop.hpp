// **********************************************************************
// smesh/include/SmeshTop.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 8 2026
/*
Top-level smesh composition point.

This component starts empty on purpose. We will add the RS, controllers, DMA
path, and local memories incrementally so the hardware block structure stays
easy to inspect.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "DmaReader.hpp"
#include "LdCtrl.hpp"
#include "MvinPixelRepeater.hpp"
#include "MvinScale.hpp"
#include "SmeshCmdQueue.hpp"
#include "SmeshRS.hpp"
#include "SmeshUnrolledCmdQueue.hpp"
#include "Spad.hpp"
#include "smem/MemTypes.hpp"

namespace smesh {

class SmeshTop : public Component {
  DECLARE_COMPONENT(SmeshTop);

 public:
  SmeshTop(std::string name, COMPONENT_CTOR);
  ~SmeshTop() override;

  Clock(clk);

  FifoInput(SmeshCmd, cmd_in); // input to SmeshTop from outside world
  FifoOutput(smem::MemReq, m_req);
  FifoInput(smem::MemResp, m_resp);

  void update();
  void reset();

 private:
  SmeshCmdQueue*           cmd_queue_ = nullptr;
  SmeshUnrolledCmdQueue*   unrolled_cmd_queue_ = nullptr;
  SmeshRS*                 rs_ = nullptr;
  LdCtrl*                  ld_ctrl_ = nullptr;
  DmaReader*               dma_reader_ = nullptr;
  MvinScale*               mvin_scale_ = nullptr;
  MvinPixelRepeater*       pixel_repeater_ = nullptr;
  Spad*                    spad_ = nullptr;
};

} // namespace smesh
