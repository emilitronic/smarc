// **********************************************************************
// smesh/src/MvinScale.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Load-path scaling stage implementation.
*/

#include "MvinScale.hpp"

namespace smesh {

MvinScale::MvinScale(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(data_in).writes(data_out);
}

void MvinScale::update() {
  if (data_in.empty() || data_out.full()) {
    return;
  }

  const auto data = data_in.pop();
  data_out.push(data);
  trace("mvin_scale: identity data cmd_id=%u last=%u", static_cast<unsigned>(data.cmd_id), static_cast<unsigned>(data.last));
}

} // namespace smesh
