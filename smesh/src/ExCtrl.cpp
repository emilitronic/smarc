// **********************************************************************
// smesh/src/ExCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026

#include "ExCtrl.hpp"

namespace smesh {

ExCtrl::ExCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateCommandPipeline).reads(cmd_in).writes(completed);
  UPDATE(updateReadPorts).writes(spad_read_req_val,
                                 spad_read_req_bits,
                                 spad_read_resp_rdy,
                                 accum_read_req_val,
                                 accum_read_req_bits,
                                 accum_read_resp_rdy);
  UPDATE(updateWritePorts).writes(spad_write_val,
                                  spad_write_bits,
                                  accum_write_val,
                                  accum_write_bits);
}

void ExCtrl::updateCommandPipeline() {
  if (active_valid_) {
    if (completed.full()) {
      return;
    }

    completed.push(active_.rs_tag);
    trace("ex_ctrl: completed placeholder cmd tag=%u funct=%u",
          static_cast<unsigned>(active_.rs_tag),
          static_cast<unsigned>(active_.cmd.funct));
    active_valid_ = false;
    return;
  }

  if (cmd_in.empty()) {
    return;
  }

  active_ = cmd_in.pop();
  active_valid_ = true;
  trace("ex_ctrl: accepted cmd tag=%u funct=%u",
        static_cast<unsigned>(active_.rs_tag),
        static_cast<unsigned>(active_.cmd.funct));
}

void ExCtrl::updateReadPorts() {
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_read_req_val[bank] = 0;
    spad_read_req_bits[bank] = SpadReadReq{};
    spad_read_resp_rdy[bank] = 0;
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_read_req_val[bank] = 0;
    accum_read_req_bits[bank] = AccumReadReq{};
    accum_read_resp_rdy[bank] = 0;
  }
}

void ExCtrl::updateWritePorts() {
  spad_write_val = 0;
  spad_write_bits = DmaReadResp{};

  accum_write_val = 0;
  accum_write_bits = DmaReadResp{};
}

void ExCtrl::reset() {
  active_valid_ = false;
  active_ = SmeshIssue{};

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_read_req_val[bank].reset(0);
    spad_read_req_bits[bank].reset(SpadReadReq{});
    spad_read_resp_rdy[bank].reset(0);
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_read_req_val[bank].reset(0);
    accum_read_req_bits[bank].reset(AccumReadReq{});
    accum_read_resp_rdy[bank].reset(0);
  }

  spad_write_val.reset(0);
  spad_write_bits.reset(DmaReadResp{});

  accum_write_val.reset(0);
  accum_write_bits.reset(DmaReadResp{});
}

} // namespace smesh
