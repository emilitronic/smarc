// **********************************************************************
// smesh/include/loop_conv/LoopConv.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Buffers commands, decodes loop setup, and saves it in two loop banks.
Ordinary commands pass through while neither bank is configured.
Bias, weights, and input workers share the command output; retirement comes later.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"
#include "LoopConvCmdDec.hpp"
#include "LoopConvState.hpp"
#include "LoopConvScheduler.hpp"
#include "LoopConvLdWeight.hpp"

namespace smesh {

class LoopConvCmdQueue;

class LoopConv : public Component {
  DECLARE_COMPONENT(LoopConv);

 public:
  LoopConv(std::string name, COMPONENT_CTOR);
  ~LoopConv() override;

  Clock(clk);

  Input(bit,            in_val);
  Input(SmeshQueuedCmd, in_bits);
  Output(bit,           in_rdy);

  Output(bit,            out_val);
  Output(SmeshQueuedCmd, out_bits);
  Input(bit,             out_rdy);
  Output(bit,            busy);
  Output(LoopConvSlot,   loop0);
  Output(LoopConvSlot,   loop1);
  Output(u8,             head_loop_id);
  Output(u8,             tail_loop_id);
  Output(u8,             config_loop_id);

  void updateRouting();
  void updateWorkerReady();
  void reset();

 private:
  LoopConvCmdQueue*  cmd_queue_  = nullptr;
  LoopConvCmdDec*    cmd_dec_    = nullptr;
  LoopConvState*     loop_state_ = nullptr;
  LoopConvScheduler* scheduler_  = nullptr;
  LoopConvLdBias*    ld_bias_    = nullptr;
  LoopConvLdInput*   ld_input_   = nullptr;
  LoopConvLdWeight*  ld_weights_ = nullptr;

  Input(bit,                head_val_);
  Input(SmeshQueuedCmd,     head_bits_);
  Output(bit,               head_rdy_);
  Input(LoopConvDecodedCmd, decoded_);
  Input(bit,                state_cmd_rdy_);
  Input(bit,                loop_configured_);
  Input(bit,                ld_bias_cmd_val_);
  Input(SmeshCmd,           ld_bias_cmd_bits_);
  Input(bit,                ld_input_cmd_val_);
  Input(SmeshCmd,           ld_input_cmd_bits_);
  Input(bit,                ld_weights_cmd_val_);
  Input(SmeshCmd,           ld_weights_cmd_bits_);
  Output(bit,               ld_bias_cmd_rdy_);
  Output(bit,               ld_input_cmd_rdy_);
  Output(bit,               ld_weights_cmd_rdy_);
};

} // namespace smesh
