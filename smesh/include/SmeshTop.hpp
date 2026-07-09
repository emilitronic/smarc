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
  // accessor fns. let testbench connect to the command queue and memory interfaces
  // e.g., top.cmdIn() << driver.cmd_out; 
  auto& cmdIn() { return unrolled_cmd_queue_->cmd_in; }  // when top.cmdIn() is called, give them cmd_in of unrolled_cmd_queue_
  auto& memReq() { return dma_reader_->mem_req; }        // when top.memReq() is called, give them mem_req of dma_reader_
  auto& memResp() { return dma_reader_->mem_resp; }      // when top.memResp() is called, give them mem_resp of dma_reader_

  // narrow inspection accessors for testbench to check internal state
  const SmeshRS& rs()     const { return *rs_; }
  const LdCtrl&  ldCtrl() const { return *ld_ctrl_; }
  const Spad&    spad()   const { return *spad_; }

  void update();
  void reset();

 private:
  SmeshUnrolledCmdQueue*   unrolled_cmd_queue_ = nullptr;
  SmeshRS*                 rs_ = nullptr;
  LdCtrl*                  ld_ctrl_ = nullptr;
  DmaReader*               dma_reader_ = nullptr;
  MvinScale*               mvin_scale_ = nullptr;
  MvinPixelRepeater*       pixel_repeater_ = nullptr;
  Spad*                    spad_ = nullptr;
};

} // namespace smesh
