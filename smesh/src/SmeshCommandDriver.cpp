// **********************************************************************
// smesh/src/SmeshCommandDriver.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026

#include "SmeshCommandDriver.hpp"

namespace smesh {

SmeshCommandDriver::SmeshCommandDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update_issue).writes(cmd_out);
  UPDATE(update_resp).reads(resp_in);
}

void SmeshCommandDriver::setScript(const std::vector<SmeshCmd>& script) {
  script_ = script;
  responses_.clear();
  pc_ = 0;
  pending_ = false;
  failed_ = false;
}

void SmeshCommandDriver::update_issue() {
  if (pending_ || pc_ >= script_.size() || cmd_out.full()) {
    return;
  }

  const auto& cmd = script_.at(pc_);
  cmd_out.push(cmd);
  trace("smesh_driver: sent cmd pc=%llu funct=%u",
        static_cast<unsigned long long>(pc_),
        static_cast<unsigned>(cmd.funct));
  ++pc_;
  pending_ = true;
}

void SmeshCommandDriver::update_resp() {
  if (resp_in.empty()) {
    return;
  }

  const auto resp = resp_in.pop();
  responses_.push_back(resp);
  pending_ = false;
  if (resp.status != 0) {
    failed_ = true;
  }
  trace("smesh_driver: got resp status=%u", static_cast<unsigned>(resp.status));
}

void SmeshCommandDriver::reset() {
  responses_.clear();
  pc_ = 0;
  pending_ = false;
  failed_ = false;
}

} // namespace smesh
