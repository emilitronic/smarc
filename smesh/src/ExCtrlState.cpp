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

  UPDATE(update)
    .reads(head_val, head_bits)
    .reads(do_config, do_preloads, do_computes)
    .reads(matmul_in_progress, pending_completed_val)
    .reads(raw_hazards_are_impossible, raw_hazard_pre)
    .reads(control_state)
    .reads(perform_single_preload)
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

  performing_single_preload   = bit(perform_single_preload == 1 && fsm_state == ExCtrlFsmState::Compute);
   
  switch (fsm_state) {
    case ExCtrlFsmState::WaitingForCmd: {
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
      }
      else if (do_preloads[0] == 1 && head_val[0] == 1 &&  head_val[1] == 1 && 
              (raw_hazards_are_impossible == 1 || raw_hazard_pre == 0)) {
        perform_single_preload_reg_ = 1;
        performing_single_preload   = 1;
        control_state_reg_          = static_cast<std::uint8_t>(ExCtrlFsmState::Compute);
      }
      break;
    }
    case ExCtrlFsmState::Compute: {
      break;
    }
    case ExCtrlFsmState::Flush: {
      break;
    }
    case ExCtrlFsmState::Flushing: {
      break;
    }
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

  config_val.reset(0);
  config_rs_tag_val.reset(0);
  config_rs_tag.reset(0);
}

} // namespace smesh
