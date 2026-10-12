// **********************************************************************
// smesh/include/loop_conv/LoopConv.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Buffers commands, decodes loop setup, and saves it in two loop banks.
Ordinary commands pass through while neither bank is configured.
All five workers share the command output and obey RS utilization limits.
Loop-slot retirement is not implemented yet.
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
class LoopConvUtilization;

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
  // Completion counts from the RS, restricted to LoopConv-originated commands.
  Input(u8,              ld_completed);
  Input(u8,              ex_completed);
  Input(u8,              st_completed);
  Output(u16,            ld_outstanding);
  Output(u16,            ex_outstanding);
  Output(u16,            st_outstanding);
  Output(bit,            busy);
  Output(LoopConvSlot,   loop0);
  Output(LoopConvSlot,   loop1);
  Output(u8,             head_loop_id);
  Output(u8,             tail_loop_id);
  Output(u8,             config_loop_id);

  void updateRouting();
  void updateWorkerReady();
  void updateExecuteProgress();
  void updateStoreProgress();
  void updateCommandTransfers();
  void reset();

 private:
  LoopConvCmdQueue*  cmd_queue_  = nullptr;
  LoopConvCmdDec*    cmd_dec_    = nullptr;
  LoopConvState*     loop_state_ = nullptr;
  LoopConvScheduler* scheduler_  = nullptr;
  LoopConvLdBias*    ld_bias_    = nullptr;
  LoopConvLdInput*   ld_input_   = nullptr;
  LoopConvLdWeight*  ld_weights_ = nullptr;
  LoopConvExecute*   ex_ = nullptr;
  LoopConvSt*        st_ = nullptr;
  LoopConvUtilization* utilization_ = nullptr;
  Output(bit, ld_cmd_fire_);
  Output(bit, ex_cmd_fire_);
  Output(bit, st_cmd_fire_);

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
  Input(bit,                ex_cmd_val_);
  Input(SmeshCmd,           ex_cmd_bits_);
  Output(bit,               ex_cmd_rdy_);
  Input(u8,                 ex_worker_loop_id_);
  Input(u8,                 bias_worker_loop_id_);
  Input(u8,                 input_worker_loop_id_);
  Input(u8,                 weights_worker_loop_id_);
  Input(bit,                bias_idle_);
  Input(bit,                input_idle_);
  Input(bit,                weights_idle_);
  Input(LoopConvSlot,       progress_loop0_);
  Input(LoopConvSlot,       progress_loop1_);
  Output(bit,               lda_completed_);
  Output(bit,               ldb_completed_);
  Output(bit,               ldd_completed_);
  Output(bit,               bias_wait_);
  Output(bit,               input_wait_);
  Output(bit,               weights_wait_);
  Input(bit,                st_cmd_val_);
  Input(SmeshCmd,           st_cmd_bits_);
  Output(bit,               st_cmd_rdy_);
  Input(u8,                 st_worker_loop_id_);
  Input(bit,                ex_idle_);
  Output(bit,               store_ex_completed_);
};

} // namespace smesh
