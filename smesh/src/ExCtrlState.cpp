// **********************************************************************
// smesh/src/ExCtrlState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 5 2026

#include "ExCtrlState.hpp"

namespace smesh {

ExCtrlState::ExCtrlState(std::string /*name*/, IMPL_CTOR) {
  //                      Q <= D
  control_state             <= control_state_D_;
  config_initialized_Q_     <= config_initialized_D_;
  shift                     <= in_shift_D_;
  activation                <= activation_D_;
  acc_scale                 <= acc_scale_D_;
  a_transpose               <= a_transpose_D_;
  bd_transpose              <= bd_transpose_D_;
  current_dataflow          <= current_dataflow_D_;
  a_addr_stride             <= a_addr_stride_D_;
  c_addr_stride             <= c_addr_stride_D_;
  ocol                      <= ocol_D_;
  kdim2                     <= kdim2_D_;
  krow                      <= krow_D_;
  channel                   <= channel_D_;
  weight_stride             <= weight_stride_D_;
  weight_double_bank        <= weight_double_bank_D_;
  weight_triple_bank        <= weight_triple_bank_D_;
  row_left                  <= row_left_D_;
  row_turn                  <= row_turn_D_;

  perform_single_preload_Q_ <= perform_single_preload_D_;
  perform_mul_pre_Q_        <= perform_mul_pre_D_;
  perform_single_mul_Q_     <= perform_single_mul_D_;

  in_prop_flush_Q_          <= in_prop_flush_D_;

  UPDATE(updateStartInputting)
    .reads(control_state)
    .reads(perform_single_preload_Q_)
    .reads(a_should_be_fed_into_transposer, b_should_be_fed_into_transposer)
    .writes(start_inputting_a, start_inputting_b, start_inputting_d);

  UPDATE(update)
    .reads(head_val, head_bits).reads(do_config, do_preloads, do_computes)
    .reads(matmul_in_progress, pending_completed_val)
    .reads(raw_hazards_are_impossible, raw_hazard_pre)
    .reads(control_state, perform_single_preload_Q_, perform_mul_pre_Q_, perform_single_mul_Q_)
    .reads(c_address_rs2)
    .reads(about_to_fire_all_rows)
    .reads(in_prop_flush_Q_, in_prop)
    .reads(current_dataflow)
    .writes(cmd_pop_count)
    .writes(control_state_D_, perform_single_preload_D_, perform_mul_pre_D_, perform_single_mul_D_)
    .writes(config_initialized_D_, in_shift_D_, activation_D_, acc_scale_D_)
    .writes(a_transpose_D_, bd_transpose_D_, current_dataflow_D_)
    .writes(a_addr_stride_D_, c_addr_stride_D_)
    .writes(ocol_D_, kdim2_D_, krow_D_, channel_D_, weight_stride_D_)
    .writes(weight_double_bank_D_, weight_triple_bank_D_, row_left_D_, row_turn_D_)
    .writes(in_prop_flush_D_)
    .writes(performing_single_preload, performing_mul_pre, performing_single_mul)
    .writes(computing, prop)
    .writes(config_val, config_rs_tag_val, config_rs_tag)
    .writes(pending_completed_set_val, pending_completed_set_bits);
}

void ExCtrlState::updateStartInputting() {
  const auto fsm_state = toFsmState(*control_state);

  start_inputting_a = 0; start_inputting_b = 0; start_inputting_d = 0;
  if (perform_single_preload_Q_ == 1 && fsm_state == ExCtrlFsmState::Compute) {
    start_inputting_a = a_should_be_fed_into_transposer; start_inputting_b = b_should_be_fed_into_transposer; start_inputting_d = 1;
  }
}

void ExCtrlState::update() {
  const auto fsm_state = toFsmState(*control_state);

  performing_single_preload = bit(perform_single_preload_Q_ == 1 && fsm_state == ExCtrlFsmState::Compute);
  performing_mul_pre        = bit(perform_mul_pre_Q_        == 1 && fsm_state == ExCtrlFsmState::Compute);
  performing_single_mul     = bit(perform_single_mul_Q_     == 1 && fsm_state == ExCtrlFsmState::Compute);
  computing = performing_single_preload || performing_mul_pre || performing_single_mul;
  prop = performing_single_preload == 1 ? *in_prop_flush_Q_ : *in_prop;

  // Default one-cycle control outputs; accepted FSM branches override them.
  cmd_pop_count     = 0;
  config_val        = 0;
  config_rs_tag_val = 0;
  config_rs_tag     = 0;
  for (std::size_t i = 0; i < 2; ++i) {
    pending_completed_set_val[i]  = 0;
    pending_completed_set_bits[i] = 0;
  }

  // ///////////////////////////////////////
  // STATE TRANS (& SAME-CYCLE OUTPUTS)
  // ///////////////////////////////////////
  switch (fsm_state) {
    case ExCtrlFsmState::WaitingForCmd: 
      perform_single_preload_D_ = 0;
      perform_mul_pre_D_        = 0;
      perform_single_mul_D_     = 0;

      if (do_config == 1 && head_val[0] == 1 &&  matmul_in_progress == 0 && pending_completed_val == 0) {
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
          config_initialized_D_ = 1;
          if (!set_only_strides) {
            // TODO check for nonlinear activations
            in_shift_D_         = static_cast<std::uint8_t>(unpackConfigExInShift(rs2));
            activation_D_       = static_cast<std::uint8_t>(unpackConfigExActivation(rs1));
            acc_scale_D_        = unpackConfigExAccScale(rs1);
            a_transpose_D_      = bit(unpackConfigExATranspose(rs1));
            bd_transpose_D_     = bit(unpackConfigExBTranspose(rs1));
            current_dataflow_D_ = static_cast<std::uint8_t>(unpackConfigExDataflow(rs1));
          }
          a_addr_stride_D_ = unpackConfigExAStride(rs1);
          c_addr_stride_D_ = unpackConfigExCStride(rs2);
        } else if (type == ConfigKind::Im2Col) {
          ocol_D_               = static_cast<std::uint8_t>(rs2 >> 56);
          kdim2_D_              = static_cast<std::uint8_t>(rs2 >> 48);
          krow_D_               = static_cast<std::uint8_t>((rs2 >> 44) & 0xfu);
          channel_D_            = static_cast<std::uint32_t>((rs2 >> 23) & 0x1ffu);
          weight_stride_D_      = static_cast<std::uint8_t>((rs2 >> 20) & 0x7u);
          weight_double_bank_D_ = bit((rs1 >> 58) & 0x1u);
          weight_triple_bank_D_ = bit((rs1 >> 59) & 0x1u);
          row_left_D_           = static_cast<std::uint8_t>((rs1 >> 54) & 0xfu);
          row_turn_D_           = static_cast<std::uint32_t>((rs1 >> 42) & 0xfffu);
        }
      }

      else if (do_preloads[0] == 1 && head_val[0] == 1 &&  head_val[1] == 1 &&  (raw_hazards_are_impossible == 1 || raw_hazard_pre == 0)) {
        perform_single_preload_D_ = 1;
        performing_single_preload = 1;
        computing                 = 1;
        prop                      = *in_prop_flush_Q_;
        control_state_D_          = static_cast<std::uint8_t>(ExCtrlFsmState::Compute);
      }
      // TODO else if overlap compute and preload
      // TODO else if single mul
      // TODO else if flush
      break;
    
    case ExCtrlFsmState::Compute:
      if (perform_single_preload_Q_ == 1) {
        // combinational outputs for single preload sub-state are already set above
        if (about_to_fire_all_rows == 1) {
          cmd_pop_count    = 1;
          control_state_D_ = static_cast<std::uint8_t>(ExCtrlFsmState::WaitingForCmd);

          const auto cmdq      = *head_bits[0];
          const bool c_garbage = c_address_rs2->is_garbage();
          // signals for completion logic: if single PRELOAD is valid and C addr is garbage, then PRELOAD is complete 
          pending_completed_set_val[0]  = bit(cmdq.rs_tag_valid != 0 && c_garbage);
          pending_completed_set_bits[0] = cmdq.rs_tag;

          if (current_dataflow == kExDataflowOS) {
            in_prop_flush_D_ = bit(!c_garbage);
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
  // reset FSM configuration
  control_state_D_.reset(static_cast<std::uint8_t>(ExCtrlFsmState::WaitingForCmd));
  config_initialized_D_.reset(0);
  in_shift_D_.reset(0);
  activation_D_.reset(0);
  acc_scale_D_.reset(0);
  a_transpose_D_.reset(0);
  bd_transpose_D_.reset(0);
  current_dataflow_D_.reset(kExDataflowWS);
  a_addr_stride_D_.reset(1);
  c_addr_stride_D_.reset(1);
  ocol_D_.reset(0);
  kdim2_D_.reset(0);
  krow_D_.reset(0);
  channel_D_.reset(0);
  weight_stride_D_.reset(0);
  weight_double_bank_D_.reset(0);
  weight_triple_bank_D_.reset(0);
  row_left_D_.reset(0);
  row_turn_D_.reset(0);

  perform_single_preload_D_.reset(0);
  perform_mul_pre_D_.reset(0);
  perform_single_mul_D_.reset(0);
  performing_single_preload.reset(0);
  performing_mul_pre.reset(0);
  performing_single_mul.reset(0);

  computing.reset(0);

  in_prop_flush_D_.reset(0);
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
