// **********************************************************************
// smesh/src/SmeshCommandDriver.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026

#include "SmeshCommandDriver.hpp"

namespace smesh {
// constructor
SmeshCommandDriver::SmeshCommandDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update_issue).writes(cmd_out);
  UPDATE(update_resp).reads(resp_in);
}
// loads commmand program into the driver
void SmeshCommandDriver::setScript(const std::vector<SmeshCmd>& script) {
  script_ = script;   // load program in driver's internal storage
  responses_.clear(); // responses received so far
  pc_ = 0;            // program counter
  pending_ = false;   // true if cmd sent, but resp not yet received
  failed_ = false;    // true if any resp has nonzero status
}
// send next cmd if possible
void SmeshCommandDriver::update_issue() {
  if (pending_ || pc_ >= script_.size() || cmd_out.full()) {
    return;
  }

  const auto& cmd = script_.at(pc_); // take next cmd from script...
  cmd_out.push(cmd);                 // ...and enque it
  trace("smesh_driver: sent cmd pc=%llu funct=%u",
        static_cast<unsigned long long>(pc_),
        static_cast<unsigned>(cmd.funct));
  ++pc_;                             // increment PC
  pending_ = true;                   // mark pending
}
// check whether shell has sent response
void SmeshCommandDriver::update_resp() {
  if (resp_in.empty()) {
    return;
  }

  const auto resp = resp_in.pop(); // pop resp
  responses_.push_back(resp);      // save it
  pending_ = false;                // clear pending so next cmd can issue
  if (resp.status != 0) {
    failed_ = true;                // mark failure for nonzero resp
  }
  trace("smesh_driver: got resp status=%u", static_cast<unsigned>(resp.status));
}
// clears runtime progress, but not script (can run same script after reset)
void SmeshCommandDriver::reset() {
  responses_.clear();
  pc_ = 0;
  pending_ = false;
  failed_ = false;
}

} // namespace smesh
