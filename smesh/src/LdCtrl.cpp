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

LdCtrl::LdCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateAccept).reads(cmd_in);                 // accept load commands from RS
  UPDATE(updateIssue).writes(dma_req);                // push DMA row requests to memory controller
  UPDATE(updateDmaResponse).reads(dma_resp).writes(completed); // let LdCtrl know when memory move is complete
}

void LdCtrl::updateAccept() {
  if (active_valid_ || cmd_in.empty()) {
    return;
  }

  active_ = cmd_in.pop();
  active_valid_ = true;

  trace("ld_ctrl: accepted rob=%u funct=%u", static_cast<unsigned>(active_.rob_id), static_cast<unsigned>(active_.cmd.funct));

  const auto funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(active_.cmd.funct));
  if (funct != SmeshFunct::Mvin && funct != SmeshFunct::Mvin2 && funct != SmeshFunct::Mvin3) {
    return;
  }

  const auto local = unpackLocal(static_cast<std::uint64_t>(active_.cmd.rs2)); // local_addr in rs2
  base_vaddr_ = static_cast<std::uint64_t>(active_.cmd.rs1);
  base_laddr_ = makeLocalAddr(local.row);
  rows_ = static_cast<std::uint32_t>(local.shape.rows);
  cols_ = static_cast<std::uint32_t>(local.shape.cols);
  next_row_ = 0;
  request_in_flight_ = false;
  expected_bytes_ = rows_ * cols_;
  returned_bytes_ = 0;
  dma_response_valid_ = false;
}

void LdCtrl::updateIssue() {
  if (!active_valid_ || request_in_flight_ || next_row_ >= rows_ || dma_req.full()) {
    return;
  }

  DmaReadReq req{};
  req.vaddr = u64(base_vaddr_ + static_cast<std::uint64_t>(next_row_) * cols_);
  req.laddr = base_laddr_ + next_row_;
  req.cols = u16(static_cast<std::uint16_t>(cols_));
  req.block_stride = u16(static_cast<std::uint16_t>(kDim));
  req.cmd_id = u16(active_.rob_id);
  dma_req.push(req);         // push DMA read request to memory controller
  request_in_flight_ = true; // just pushed, so one DMA row request is outstanding
  ++next_row_;

  trace("ld_ctrl: dma request vaddr=0x%llx laddr=0x%x cols=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.cols),
        static_cast<unsigned>(req.cmd_id));
}

void LdCtrl::updateDmaResponse() {
  if (dma_resp.empty()) {
    return;
  }

  const auto& pending = dma_resp.peek();
  assert_always(active_valid_, "LdCtrl received a DMA response without an active command");
  assert_always(static_cast<std::uint16_t>(pending.cmd_id) == active_.rob_id, "LdCtrl DMA response ID does not match active command");

  const auto new_returned_bytes = returned_bytes_ + static_cast<std::uint16_t>(pending.bytes_read);
  if (new_returned_bytes >= expected_bytes_ && completed.full()) {
    return;
  }

  const auto response = dma_resp.pop();
  returned_bytes_ = new_returned_bytes;
  request_in_flight_ = false;  // just got resposne, so no DMA row request is outstanding
  response_cmd_id_ = static_cast<SmeshRobId>(response.cmd_id);
  dma_response_valid_ = true;

  trace("ld_ctrl: dma response bytes_read=%u cmd_id=%u total=%u", static_cast<unsigned>(response.bytes_read), static_cast<unsigned>(response.cmd_id), static_cast<unsigned>(returned_bytes_));

  if (returned_bytes_ >= expected_bytes_) {
    completed.push(active_.rob_id);
    active_valid_ = false;
  }
}

void LdCtrl::reset() {
  active_valid_ = false;
  active_ = {};
  dma_response_valid_ = false;
  request_in_flight_ = false;
  base_vaddr_ = 0;
  base_laddr_ = {};
  rows_ = 0;
  cols_ = 0;
  next_row_ = 0;
  expected_bytes_ = 0;
  returned_bytes_ = 0;
  response_cmd_id_ = 0;
}

} // namespace smesh
