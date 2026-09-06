// **********************************************************************
// smesh/src/ExCtrlState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 5 2026

#include "ExCtrlState.hpp"

namespace smesh {

ExCtrlState::ExCtrlState(std::string /*name*/, IMPL_CTOR) {
  control_state <= control_state_reg_;

  UPDATE(update)
    .reads(head_val, head_bits)
    .reads(do_config, do_preloads, do_computes)
    .reads(matmul_in_progress, pending_completed_val)
    
    .reads(in_prop, about_to_fire_all_rows, c_address_rs2)
    .reads(control_state, config_initialized, a_transpose, bd_transpose, current_dataflow,
           activation, acc_scale, a_addr_stride)
    .reads(c_addr_stride, shift)
    .writes(config_val, config_rs_tag_val, config_rs_tag)
    .writes(pending_completed_set_val, pending_completed_set_bits, performing_single_preload, computing)
    .writes(prop, cmd_pop_count)
    .writes(control_state_reg_)
    .reads(a_should_be_fed_into_transposer, b_should_be_fed_into_transposer)
    .writes(start_inputting_a, start_inputting_b, start_inputting_d);
}

void ExCtrlState::update() {
  const auto state = toFsmState(*control_state);
   
  switch (state) {
    case ExCtrlFsmState::WaitingForCmd: {
      if (head_val[0] == 1 && do_config == 1 && matmul_in_progress == 0 && pending_completed_val == 0) {
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
}

} // namespace smesh
