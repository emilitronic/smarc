// **********************************************************************
// smesh/src/ExCtrlState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 26 2026

#include "ExCtrlState.hpp"

namespace smesh {

ExCtrlState::ExCtrlState(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(head_val,
             head_bits,
             do_config,
             do_preloads,
             matmul_in_progress,
             pending_completed_valid,
             raw_hazards_are_impossible,
             raw_hazard_pre)
      .writes(config_initialized,
              a_transpose,
              bd_transpose,
              current_dataflow,
              a_addr_stride,
              c_addr_stride)
      .writes(
              config_val,
              config_rs_tag_valid,
              config_rs_tag,
              perform_single_preload,
              cmd_pop_count);
}

void ExCtrlState::update() {
  config_val             = 0; // FSM accepts/processes a CONFIG command this cycle
  config_rs_tag_valid    = 0;
  config_rs_tag          = 0;
  perform_single_preload = 0;
  cmd_pop_count          = 0;

  switch (state_) {
    case ExCtrlFsmState::WaitingForCmd: {
      if (head_val[0] != 0 && do_config != 0 && matmul_in_progress == 0 && pending_completed_valid == 0) {
        const auto issue = *head_bits[0];
        const auto rs1   = static_cast<std::uint64_t>(issue.cmd.rs1);
        const auto rs2   = static_cast<std::uint64_t>(issue.cmd.rs2);
        const auto kind  = static_cast<ConfigKind>(rs1 & 0x3u);
        config_val = 1;
        config_rs_tag_valid = issue.rs_tag_valid;
        config_rs_tag       = issue.rs_tag;
        cmd_pop_count       = 1;
        if (kind == ConfigKind::Execute) {
          config_initialized_ = true;
          a_transpose_        = unpackConfigExecuteATranspose(rs1);
          a_addr_stride_      = unpackConfigExecuteAStride(rs1);
          c_addr_stride_      = unpackConfigExecuteCStride(rs2);
        }
      } else if (head_val[0] != 0 && do_preloads[0] != 0 && head_val[1] != 0 &&
                 (raw_hazards_are_impossible != 0 || raw_hazard_pre == 0)) {
        perform_single_preload = 1;
      }
      break;
    }

    case ExCtrlFsmState::Compute:
      // TODO: issue operand reads and wait for all rows to enter the mesh.
      break;

    case ExCtrlFsmState::Flush:
      // TODO: send a mesh flush request.
      break;

    case ExCtrlFsmState::Flushing:
      // TODO: wait for mesh drain/flush completion.
      break;
  }

  config_initialized = bit(config_initialized_);
  a_transpose        = bit(a_transpose_);
  bd_transpose       = bit(bd_transpose_);
  current_dataflow   = current_dataflow_;
  a_addr_stride      = a_addr_stride_;
  c_addr_stride      = c_addr_stride_;
}

void ExCtrlState::reset() {
  state_              = ExCtrlFsmState::WaitingForCmd;
  config_initialized_ = false;
  a_transpose_        = false;
  bd_transpose_       = false;
  current_dataflow_   = kExDataflowWS;
  a_addr_stride_      = 1;
  c_addr_stride_      = 1;

  config_initialized.reset(0);
  a_transpose.reset(0);
  bd_transpose.reset(0);
  current_dataflow.reset(kExDataflowWS);
  a_addr_stride.reset(1);
  c_addr_stride.reset(1);
  config_val.reset(0);
  config_rs_tag_valid.reset(0);
  config_rs_tag.reset(0);
  perform_single_preload.reset(0);
  cmd_pop_count.reset(0);
}

} // namespace smesh
