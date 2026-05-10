// **********************************************************************
// smesh/src/SmeshCommandDriver.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026
/*
Cascade component that acts as a scripted driver for sending commands to the SmeshShell.
Sends one command at-a-time and waits for responses.  
This is used in the M2 testbench to run scripted sequences of commands and check responses, 
simulating how software would interact with the device.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <vector>

namespace smesh {

class SmeshCommandDriver : public Component {
  DECLARE_COMPONENT(SmeshCommandDriver);

 public:
  SmeshCommandDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoOutput(SmeshCmd, cmd_out);
  FifoInput(SmeshResp, resp_in);

  void setScript(const std::vector<SmeshCmd>& script);
  bool done() const { return pc_ >= script_.size() && !pending_; }
  bool failed() const { return failed_; }
  const std::vector<SmeshResp>& responses() const { return responses_; }

  void update_issue();
  void update_resp();
  void reset();

 private:
  std::vector<SmeshCmd> script_;
  std::vector<SmeshResp> responses_;
  std::size_t pc_ = 0;
  bool pending_ = false;
  bool failed_ = false;
};

} // namespace smesh
