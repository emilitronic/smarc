// **********************************************************************
// smesh/src/MvinScale.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Load-path scaling stage implementation.
*/

#include "MvinScale.hpp"

namespace smesh {
// scale normal width data coming from DMA reader
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
// scale accumulator-width data coming from DMA reader
MvinScaleAcc::MvinScaleAcc(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateView)
      .reads(data_in)
      .writes(data_val, data_bits);
  UPDATE(update)
      .reads(data_in, data_rdy)
      .writes(data_out);
}

void MvinScaleAcc::updateView() {
  data_val = 0;
  data_bits = DmaReadResp{};

  if (data_in.empty()) {
    return;
  }

  data_val = 1;
  data_bits = data_in.peek();
}

void MvinScaleAcc::update() {
  if (data_in.empty()) {
    return;
  }

  if (data_rdy != 0) {
    const auto data = data_in.pop();
    trace("mvin_scale_acc: explicit data cmd_id=%u last=%u",
          static_cast<unsigned>(data.cmd_id),
          static_cast<unsigned>(data.last));
    return;
  }

  if (data_out.full()) {
    return;
  }

  const auto data = data_in.pop();
  data_out.push(data);
  trace("mvin_scale_acc: identity data cmd_id=%u last=%u",
        static_cast<unsigned>(data.cmd_id),
        static_cast<unsigned>(data.last));
}
// split incoming data into normal-width path and accumulator-width path
MvinScaleSplit::MvinScaleSplit(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(data_in).writes(normal_out, acc_out);
}

void MvinScaleSplit::update() {
  if (data_in.empty()) {
    return;
  }

  const auto& pending = data_in.peek();
  const bool use_acc_path = pending.laddr.is_acc_addr() &&
                            pending.has_acc_bitwidth != 0;
  if (use_acc_path) {
    if (acc_out.full()) {
      return;
    }
    const auto data = data_in.pop();
    acc_out.push(data);
    trace("mvin_scale_split: to acc-width path cmd_id=%u",
          static_cast<unsigned>(data.cmd_id));
    return;
  }

  if (normal_out.full()) {
    return;
  }
  const auto data = data_in.pop();
  normal_out.push(data);
  trace("mvin_scale_split: to normal path cmd_id=%u",
        static_cast<unsigned>(data.cmd_id));
}

} // namespace smesh
