// **********************************************************************
// smesh/src/LdCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026
/*
Load controller implementation.
*/

#include "LdCtrl.hpp"

#include "SmeshCommand.hpp"

namespace smesh {

namespace {

std::size_t loadStateId(SmeshFunct funct) {
  switch (funct) {
    case SmeshFunct::Mvin:
      return 0;
    case SmeshFunct::Mvin2:
      return 1;
    case SmeshFunct::Mvin3:
      return 2;
    default:
      return 0;
  }
}

} // namespace

LdCtrl::LdCtrl(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateCompletionView).reads(state_Q_).writes(completed_val, completed_bits);
  UPDATE(updateState)
      .reads(state_Q_, cmd_in, dma_resp, completed_rdy)
      .writes(state_D_, dma_req);
}

void LdCtrl::updateCompletionView() {
  const auto current = *state_Q_;
  completed_val = bit(current.active_valid && current.command_done);
  completed_bits = current.active_valid && current.command_done
                       ? current.active.rs_tag : SmeshRsTag{};
}

void LdCtrl::updateState() {
  const auto current = *state_Q_;
  auto next = current;

  if (current.active_valid && current.command_done && completed_rdy == 1) {
    trace("ld_ctrl: completed tag=%u", static_cast<unsigned>(current.active.rs_tag));
    next.active_valid = false;
    next.command_done = false;
  } else if (!current.active_valid && !cmd_in.empty()) {
    next.active = cmd_in.pop();
    next.active_valid = true;
    next.command_done = false;

    trace("ld_ctrl: accepted tag=%u funct=%u",
          static_cast<unsigned>(next.active.rs_tag),
          static_cast<unsigned>(next.active.cmd.funct));

    const auto funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(next.active.cmd.funct));
    if (funct == SmeshFunct::Config) {
      const auto rs1 = static_cast<std::uint64_t>(next.active.cmd.rs1);
      const auto kind = static_cast<ConfigKind>(rs1 & 0x3u);
      assert_always(kind == ConfigKind::Load, "LdCtrl received a non-load CONFIG command");
      const auto state_id = unpackConfigStateId(rs1);
      assert_always(state_id < next.load_config.size(), "LdCtrl CONFIG load-state ID is out of range");
      next.load_config[state_id].ld_block_stride = unpackConfigLoadBlockStride(rs1);
      next.load_config[state_id].dram_row_stride = static_cast<std::uint32_t>(next.active.cmd.rs2);
      next.command_done = true;
      trace("ld_ctrl: config state=%u dram_stride=%u block_stride=%u",
            static_cast<unsigned>(state_id),
            static_cast<unsigned>(next.load_config[state_id].dram_row_stride),
            static_cast<unsigned>(next.load_config[state_id].ld_block_stride));
    } else {
      assert_always(funct == SmeshFunct::Mvin || funct == SmeshFunct::Mvin2 ||
                    funct == SmeshFunct::Mvin3, "LdCtrl received an unsupported command");
      const auto local = unpackLocal(static_cast<std::uint64_t>(next.active.cmd.rs2));
      const auto& config = current.load_config[loadStateId(funct)];
      next.base_vaddr = static_cast<std::uint64_t>(next.active.cmd.rs1);
      next.base_laddr = makeLocalAddr(local.row);
      next.rows = static_cast<std::uint32_t>(local.shape.rows);
      next.cols = static_cast<std::uint32_t>(local.shape.cols);
      next.next_row = 0;
      next.request_in_flight = false;
      next.dram_row_stride = config.dram_row_stride;
      next.ld_block_stride = config.ld_block_stride;
      next.expected_bytes = next.rows * next.cols;
      next.returned_bytes = 0;
      next.dma_response_valid = false;
    }
  }

  if (current.active_valid && !current.command_done &&
      !current.request_in_flight && current.next_row < current.rows &&
      !dma_req.full()) {
    DmaReadReq req{};
    req.vaddr = u64(current.base_vaddr +
                    static_cast<std::uint64_t>(current.next_row) * current.dram_row_stride);
    req.laddr = current.base_laddr + current.next_row;
    req.cols = u16(static_cast<std::uint16_t>(current.cols));
    req.block_stride = u16(static_cast<std::uint16_t>(current.ld_block_stride));
    req.cmd_id = u16(current.active.rs_tag);
    dma_req.push(req);
    next.request_in_flight = true;
    ++next.next_row;

    trace("ld_ctrl: dma request vaddr=0x%llx laddr=0x%x cols=%u cmd_id=%u",
          static_cast<unsigned long long>(req.vaddr),
          static_cast<unsigned>(req.laddr.raw),
          static_cast<unsigned>(req.cols),
          static_cast<unsigned>(req.cmd_id));
  }

  if (!dma_resp.empty()) {
    const auto response = dma_resp.peek();
    assert_always(current.active_valid, "LdCtrl received a DMA response without an active command");
    assert_always(static_cast<std::uint16_t>(response.cmd_id) == current.active.rs_tag,
                  "LdCtrl DMA response ID does not match active command");
    dma_resp.pop();
    next.returned_bytes = current.returned_bytes + static_cast<std::uint16_t>(response.bytes_read);
    next.request_in_flight = false;
    next.response_rs_tag = static_cast<SmeshRsTag>(response.cmd_id);
    next.dma_response_valid = true;
    next.command_done = next.returned_bytes >= current.expected_bytes;

    trace("ld_ctrl: dma response bytes_read=%u cmd_id=%u total=%u",
          static_cast<unsigned>(response.bytes_read),
          static_cast<unsigned>(response.cmd_id),
          static_cast<unsigned>(next.returned_bytes));
  }

  state_D_ = next;
}

void LdCtrl::reset() {
  State initial{};
  for (auto& config : initial.load_config) {
    config.dram_row_stride = static_cast<std::uint32_t>(kDim);
    config.ld_block_stride = static_cast<std::uint32_t>(kDim);
  }
  state_D_.reset(initial);
  completed_val.reset(0);
  completed_bits.reset(SmeshRsTag{});
}

} // namespace smesh
