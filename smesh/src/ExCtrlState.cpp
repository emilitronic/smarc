// **********************************************************************
// smesh/src/ExCtrlState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 5 2026

#include "ExCtrlState.hpp"

namespace smesh {

ExCtrlState::ExCtrlState(std::string /*name*/, IMPL_CTOR) {
  //                      Q <= D
  control_state             <= control_state_reg_;
  config_initialized        <= config_initialized_reg_;
  shift                     <= in_shift_reg_;
  activation                <= activation_reg_;
  acc_scale                 <= acc_scale_reg_;
  a_transpose               <= a_transpose_reg_;
  bd_transpose              <= bd_transpose_reg_;
  current_dataflow          <= current_dataflow_reg_;
  a_addr_stride             <= a_addr_stride_reg_;
  c_addr_stride             <= c_addr_stride_reg_;

  perform_single_preload    <= perform_single_preload_reg_;

  in_prop_flush             <= in_prop_flush_reg_;

  UPDATE(update)
    .reads(head_val, head_bits)
    .reads(do_config, do_preloads, do_computes)
    .reads(matmul_in_progress, pending_completed_val)
    .reads(raw_hazards_are_impossible, raw_hazard_pre)
    .reads(control_state)
    .reads(perform_single_preload)
    .reads(in_prop_flush)
    .writes(performing_single_preload)

    .reads(in_prop, about_to_fire_all_rows, c_address_rs2)
    .reads(config_initialized, a_transpose, bd_transpose, current_dataflow,
           activation, acc_scale, a_addr_stride)
    .reads(c_addr_stride, shift)
    .writes(config_val, config_rs_tag_val, config_rs_tag)
    .writes(pending_completed_set_val, pending_completed_set_bits, computing)
    .writes(prop, cmd_pop_count)
    .writes(control_state_reg_, perform_single_preload_reg_)
    .reads(a_should_be_fed_into_transposer, b_should_be_fed_into_transposer)
    .writes(start_inputting_a, start_inputting_b, start_inputting_d);
}

void ExCtrlState::update() {
  const auto fsm_state = toFsmState(*control_state);
  // ///////////////////////////////////////
  // COMBINATIONAL OUTPUTS
  // ///////////////////////////////////////
  performing_single_preload = bit(perform_single_preload == 1 && fsm_state == ExCtrlFsmState::Compute);
  if (perform_single_preload == 1 && fsm_state == ExCtrlFsmState::Compute) { start_inputting_a = a_should_be_fed_into_transposer; start_inputting_b = b_should_be_fed_into_transposer; start_inputting_d = 1;
  } else {  start_inputting_a = 0; start_inputting_b = 0; start_inputting_d = 0;}

  // ///////////////////////////////////////
  // STATE TRANS (& SAME-CYCLE OUTPUTS)
  // ///////////////////////////////////////
  switch (fsm_state) {
    case ExCtrlFsmState::WaitingForCmd: 
      perform_single_preload_reg_ = 0;

      if (do_config == 1 && head_val[0] == 1 && 
          matmul_in_progress == 0 && pending_completed_val == 0) {

        const auto cmdq  = *head_bits[0];
        const auto rs1   = rawRs1(cmdq);
        const auto rs2   = rawRs2(cmdq);
        const auto type  = configType(rs1); // type of config

        config_val        = 1;
        config_rs_tag_val = cmdq.rs_tag_valid;
        config_rs_tag     = cmdq.rs_tag;
        cmd_pop_count     = 1; // tell cmd q how many entries to pop (1 for CONFIG_EX)

        if (type == ConfigKind::Execute) {
          const bool set_only_strides = unpackConfigExSetOnlyStrides(rs1);
          config_initialized_reg_ = 1;
          if (!set_only_strides) {
            // TODO check for nonlinear activations
            in_shift_reg_         = static_cast<std::uint8_t>(unpackConfigExInShift(rs2));
            activation_reg_       = static_cast<std::uint8_t>(unpackConfigExActivation(rs1));
            acc_scale_reg_        = unpackConfigExAccScale(rs1);
            a_transpose_reg_      = bit(unpackConfigExATranspose(rs1));
            bd_transpose_reg_     = bit(unpackConfigExBTranspose(rs1));
            current_dataflow_reg_ = static_cast<std::uint8_t>(unpackConfigExDataflow(rs1));
          }
          a_addr_stride_reg_ = unpackConfigExAStride(rs1);
          c_addr_stride_reg_ = unpackConfigExCStride(rs2);
        }
      } // TODO else if CONFIG_IM2COL
      else if (do_preloads[0] == 1 && head_val[0] == 1 &&  head_val[1] == 1 && 
              (raw_hazards_are_impossible == 1 || raw_hazard_pre == 0)) {

        perform_single_preload_reg_ = 1;
        performing_single_preload   = 1;
        control_state_reg_          = static_cast<std::uint8_t>(ExCtrlFsmState::Compute);
      }
      // TODO else if overlap compute and preload
      // TODO else if single mul
      // TODO else if flush
      break;
    
    case ExCtrlFsmState::Compute:
      if (perform_single_preload == 1) {
        // combinational outputs for single preload sub-state are already set above
        if (about_to_fire_all_rows == 1) {
          cmd_pop_count = 1;
          control_state_reg_ = static_cast<std::uint8_t>(ExCtrlFsmState::WaitingForCmd);

          const auto cmdq      = *head_bits[0];
          const bool c_garbage = c_address_rs2->is_garbage();
          // signals for completion logic: if single PRELOAD is valid and C addr is garbage, then PRELOAD is complete 
          pending_completed_set_val[0]  = bit(cmdq.rs_tag_valid != 0 && c_garbage);
          pending_completed_set_bits[0] = cmdq.rs_tag;

          if (current_dataflow == kExDataflowOS) {
            in_prop_flush_reg_ = bit(!c_garbage);
          }

        }
      }
      break;
  
    case ExCtrlFsmState::Flush: 
      break;
    
    case ExCtrlFsmState::Flushing: 
      break;
    
  }
}

void ExCtrlState::reset() {
  control_state_reg_.reset(static_cast<std::uint8_t>(ExCtrlFsmState::WaitingForCmd));
  config_initialized_reg_.reset(0);
  in_shift_reg_.reset(0);
  activation_reg_.reset(0);
  acc_scale_reg_.reset(0);
  a_transpose_reg_.reset(0);
  bd_transpose_reg_.reset(0);
  current_dataflow_reg_.reset(kExDataflowWS);
  a_addr_stride_reg_.reset(1);
  c_addr_stride_reg_.reset(1);

  perform_single_preload_reg_.reset(0);
  performing_single_preload.reset(0);

  in_prop_flush_reg_.reset(0);
  // reset output to completion block
  config_val.reset(0);
  config_rs_tag_val.reset(0);
  config_rs_tag.reset(0);
  for (std::size_t i = 0; i < 2; ++i) {
    pending_completed_set_val[i].reset(0);
    pending_completed_set_bits[i].reset(0);
  }
}

} // namespace smesh
