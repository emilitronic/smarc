// **********************************************************************
// smesh/src/ExCtrlState2.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 6 2026

#include "ExCtrlState2.hpp"

namespace smesh {

ExCtrlState2::ExCtrlState2(std::string /*name*/, IMPL_CTOR) {
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

  // .reads() & .writes() should guide Cascade's dependency graph to execute our updates
  // in the ricth order, updateCmdAcceptanceAndOutputs() first, then updateState() second.
  UPDATE(updateCmdAcceptanceAndOutputs)
      .reads(head_val, do_config, do_preloads)
      .reads(matmul_in_progress, pending_completed_val)
      .reads(raw_hazards_are_impossible, raw_hazard_pre)
      .reads(control_state)
      .reads(perform_single_preload_Q_, perform_mul_pre_Q_, perform_single_mul_Q_)
      .reads(a_should_be_fed_into_transposer, b_should_be_fed_into_transposer)
      .reads(in_prop_flush_Q_, in_prop)
      .writes(accepting_config_, accepting_single_preload_)
      .writes(performing_single_preload, performing_mul_pre, performing_single_mul)
      .writes(start_inputting_a, start_inputting_b, start_inputting_d)
      .writes(computing, prop);

  UPDATE(updateState)
      .reads(accepting_config_, accepting_single_preload_)
      .reads(head_bits, about_to_fire_all_rows, c_address_rs2, current_dataflow)
      .reads(control_state, perform_single_preload_Q_)
      .writes(cmd_pop_count)
      .writes(control_state_D_)
      .writes(perform_single_preload_D_, perform_mul_pre_D_, perform_single_mul_D_)
      .writes(config_initialized_D_, in_shift_D_, activation_D_, acc_scale_D_)
      .writes(a_transpose_D_, bd_transpose_D_, current_dataflow_D_)
      .writes(a_addr_stride_D_, c_addr_stride_D_)
      .writes(ocol_D_, kdim2_D_, krow_D_, channel_D_, weight_stride_D_)
      .writes(weight_double_bank_D_, weight_triple_bank_D_, row_left_D_, row_turn_D_)
      .writes(in_prop_flush_D_)
      .writes(config_val, config_rs_tag_val, config_rs_tag)
      .writes(pending_completed_set_val, pending_completed_set_bits);
}

// Select the current action once, then derive each immediate output once.
void ExCtrlState2::updateCmdAcceptanceAndOutputs() {
  const auto fsm_state = toFsmState(*control_state);
  const bool waiting   = fsm_state == ExCtrlFsmState::WaitingForCmd;

  const bool accepting_config         = waiting && head_val[0] == 1 && do_config == 1 &&  matmul_in_progress == 0 && pending_completed_val == 0;
  const bool accepting_single_preload = waiting && !accepting_config && head_val[0] == 1 && do_preloads[0] == 1 && head_val[1] == 1 && (raw_hazards_are_impossible == 1 || raw_hazard_pre == 0);

  accepting_config_         = bit(accepting_config);
  accepting_single_preload_ = bit(accepting_single_preload);

  const bool active_single_preload = fsm_state == ExCtrlFsmState::Compute && perform_single_preload_Q_ == 1;
  const bool active_mul_pre        = fsm_state == ExCtrlFsmState::Compute && perform_mul_pre_Q_ == 1;
  const bool active_single_mul     = fsm_state == ExCtrlFsmState::Compute && perform_single_mul_Q_ == 1;

  performing_single_preload = bit(active_single_preload || accepting_single_preload);
  performing_mul_pre        = bit(active_mul_pre);
  performing_single_mul     = bit(active_single_mul);

  start_inputting_a = 0; start_inputting_b = 0; start_inputting_d = 0;
  if (active_single_preload || accepting_single_preload) {
    start_inputting_a = a_should_be_fed_into_transposer;
    start_inputting_b = b_should_be_fed_into_transposer;
    start_inputting_d = 1;
  }

  computing = bit(active_single_preload || accepting_single_preload || active_mul_pre || active_single_mul);
  prop = performing_single_preload == 1 ? *in_prop_flush_Q_ : *in_prop;
}

// Update register D values and emit command/completion events from the decision.
void ExCtrlState2::updateState() {
  const auto fsm_state = toFsmState(*control_state);

  cmd_pop_count     = 0;
  config_val        = 0;
  config_rs_tag_val = 0;
  config_rs_tag     = 0;
  for (std::size_t i = 0; i < 2; ++i) {
    pending_completed_set_val[i]  = 0;
    pending_completed_set_bits[i] = 0;
  }

  switch (fsm_state) {
    case ExCtrlFsmState::WaitingForCmd:
      perform_single_preload_D_ = 0;
      perform_mul_pre_D_        = 0;
      perform_single_mul_D_     = 0;

      if (accepting_config_ == 1) {
        const auto cmdq = *head_bits[0];
        const auto rs1  = rawRs1(cmdq);
        const auto rs2  = rawRs2(cmdq);
        const auto type = configType(rs1);

        config_val        = 1;
        config_rs_tag_val = cmdq.rs_tag_valid;
        config_rs_tag     = cmdq.rs_tag;
        cmd_pop_count     = 1;

        if (type == ConfigKind::Execute) {
          const bool set_only_strides = unpackConfigExSetOnlyStrides(rs1);
          config_initialized_D_ = 1;
          if (!set_only_strides) {
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
      } else if (accepting_single_preload_ == 1) {
        perform_single_preload_D_ = 1;
        control_state_D_ = static_cast<std::uint8_t>(ExCtrlFsmState::Compute);
      }
      break;

    case ExCtrlFsmState::Compute:
      if (perform_single_preload_Q_ == 1 && about_to_fire_all_rows == 1) {
        cmd_pop_count    = 1;
        control_state_D_ = static_cast<std::uint8_t>(ExCtrlFsmState::WaitingForCmd);

        const auto cmdq      = *head_bits[0];
        const bool c_garbage = c_address_rs2->is_garbage();
        pending_completed_set_val[0]  = bit(cmdq.rs_tag_valid != 0 && c_garbage);
        pending_completed_set_bits[0] = cmdq.rs_tag;

        if (current_dataflow == kExDataflowOS) {
          in_prop_flush_D_ = bit(!c_garbage);
        }
      }
      break;

    case ExCtrlFsmState::Flush:
      break;

    case ExCtrlFsmState::Flushing:
      break;
  }
}

void ExCtrlState2::reset() {
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
  in_prop_flush_D_.reset(0);

  accepting_config_.reset(0);
  accepting_single_preload_.reset(0);
  performing_single_preload.reset(0);
  performing_mul_pre.reset(0);
  performing_single_mul.reset(0);
  start_inputting_a.reset(0);
  start_inputting_b.reset(0);
  start_inputting_d.reset(0);
  computing.reset(0);
  prop.reset(0);

  cmd_pop_count.reset(0);
  config_val.reset(0);
  config_rs_tag_val.reset(0);
  config_rs_tag.reset(0);
  for (std::size_t i = 0; i < 2; ++i) {
    pending_completed_set_val[i].reset(0);
    pending_completed_set_bits[i].reset(0);
  }
}

} // namespace smesh
