// **********************************************************************
// smesh/include/SpadReadPipes.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
Scratchpad read response pipes.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class SpadDmaReadPipe : public Component {
  DECLARE_COMPONENT(SpadDmaReadPipe);

 public:
  SpadDmaReadPipe(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SpadReadResp, resp_in);
  Output(bit, out_val);
  Output(SpadReadResp, out_bits);
  Input(bit, out_rdy);

  void updateOutView();
  void updateOutPop();
  void updateAccept();
  void reset();
  // inspection accessors for testbench to check pipe state
  bool hasAcceptedResponse() const { return accepted_response_; }
  const SpadReadResp& lastResponse() const { return last_response_; }

 private:
  bool accepted_response_ = false; // did spad DMA read pipe accept SpadReadResp since reset
  SpadReadResp last_response_{};   // stores copy of most recent resp pipe accepted
  bool out_valid_ = false;
  SpadReadResp out_entry_{};
};

class ExDmaReadPipe : public Component {
  DECLARE_COMPONENT(ExDmaReadPipe);

 public:
  ExDmaReadPipe(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SpadReadResp, resp_in);
  FifoOutput(SpadReadResp, resp_out);

  void update();
};

} // namespace smesh
