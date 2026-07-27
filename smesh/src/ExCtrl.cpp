// **********************************************************************
// smesh/src/ExCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026

#include "ExCtrl.hpp"

namespace smesh {

ExCtrl::ExCtrl(std::string /*name*/, IMPL_CTOR) {
  cmd_queue_   = new ExCtrlCmdQueue("ExCtrlCmdQueue");
  cmd_decoder_ = new ExCtrlDecoder("ExCtrlDecoder");
  cmd_state_   = new ExCtrlState("ExCtrlState");

  cmd_queue_->clk   << clk;
  cmd_decoder_->clk << clk;
  cmd_state_->clk   << clk;
  
  cmd_queue_->cmd_in << cmd_in;
  cmd_queue_->pop_count << cmd_queue_pop_count_;
  for (std::size_t i = 0; i < kExCtrlCmdWindow; ++i) {
    cmd_decoder_->head_val[i]  << cmd_queue_->head_val[i];
    cmd_decoder_->head_bits[i] << cmd_queue_->head_bits[i];
    cmd_state_->head_val[i]    << cmd_queue_->head_val[i];
    cmd_state_->head_bits[i]   << cmd_queue_->head_bits[i];
    cmd_state_->do_preloads[i] << cmd_decoder_->do_preloads[i];
    cmd_state_->do_computes[i] << cmd_decoder_->do_computes[i];
  }
  cmd_decoder_->current_dataflow <= cmd_state_->current_dataflow; // dec gets FSM configs
  cmd_decoder_->a_transpose      <= cmd_state_->a_transpose;      // 
  cmd_decoder_->bd_transpose     <= cmd_state_->bd_transpose;     //
  cmd_decoder_->ex_read_from_acc << decoder_ex_read_from_acc_;    // const from SmeshConfig.hpp
  cmd_decoder_->ex_write_to_spad << decoder_ex_write_to_spad_;    // const from SmeshConfig.hpp

  cmd_state_->do_config << cmd_decoder_->do_config;

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
  UPDATE(updateDecoderInputs).writes(decoder_ex_read_from_acc_,
                                     decoder_ex_write_to_spad_);
}

ExCtrl::~ExCtrl() {
  delete cmd_state_;
  delete cmd_decoder_;
  delete cmd_queue_;
}

void ExCtrl::updateCommandPipeline() {
  cmd_queue_pop_count_ = 0;

  if (cmd_queue_->head_val[0] == 0) {
    return;
  }

  const auto issue = *cmd_queue_->head_bits[0];
  if (issue.rs_tag_valid != 0 && completed.full()) {
    return;
  }
  if (issue.rs_tag_valid) {
    completed.push(issue.rs_tag);
  }
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

void ExCtrl::updateDecoderInputs() {
  decoder_ex_read_from_acc_ = bit(kDefaultConfig.ex_read_from_acc);
  decoder_ex_write_to_spad_ = bit(kDefaultConfig.ex_write_to_spad);
}

void ExCtrl::reset() {
  cmd_queue_pop_count_.reset(0);
  decoder_ex_read_from_acc_.reset(bit(kDefaultConfig.ex_read_from_acc));
  decoder_ex_write_to_spad_.reset(bit(kDefaultConfig.ex_write_to_spad));

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
