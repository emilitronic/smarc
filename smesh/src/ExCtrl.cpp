// **********************************************************************
// smesh/src/ExCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026

#include "ExCtrl.hpp"

namespace smesh {

ExCtrl::ExCtrl(std::string /*name*/, IMPL_CTOR) {
  cmd_queue_ = new ExCtrlCmdQueue("ExCtrlCmdQueue");
  cmd_queue_->clk << clk;
  cmd_queue_->cmd_in << cmd_in;
  cmd_queue_->pop_count << cmd_queue_pop_count_;

  UPDATE(updateCommandPipeline)
      .reads(cmd_queue_->head_val[0], cmd_queue_->head_bits[0])
      .writes(completed, cmd_queue_pop_count_);
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

ExCtrl::~ExCtrl() {
  delete cmd_queue_;
}

void ExCtrl::updateCommandPipeline() {
  cmd_queue_pop_count_ = 0;

  if (cmd_queue_->head_val[0] == 0 || completed.full()) {
    return;
  }

  const auto issue = *cmd_queue_->head_bits[0];
  completed.push(issue.rs_tag);
  cmd_queue_pop_count_ = 1;

  trace("ex_ctrl: completed placeholder cmd tag=%u funct=%u",
        static_cast<unsigned>(issue.rs_tag),
        static_cast<unsigned>(issue.cmd.funct));
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
  cmd_queue_pop_count_.reset(0);

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
