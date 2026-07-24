// **********************************************************************
// smesh/src/StCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026

#include "StCtrl.hpp"

#include "SmeshCommand.hpp"

namespace smesh {

StCtrl::StCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateDispatch).reads(cmd_in).writes(dma_req);
  UPDATE(updateComplete).reads(dma_resp).writes(completed);
}

void StCtrl::updateDispatch() {
  if (cmd_in.empty() || dma_req.full()) {
    return;
  }

  const auto issue = cmd_in.pop();
  const auto funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(issue.cmd.funct)); // convert to enum class type
  const auto local = unpackLocal(static_cast<std::uint64_t>(issue.cmd.rs2));
  const bool dst_is_spad = funct == SmeshFunct::StoreSpad; // if funct=StoreSpad, then store in external spad, otherwise in main mem

  DmaWriteReq req{};
  req.vaddr    = issue.cmd.rs1;
  req.laddr    = makeLocalAddr(local.row);
  req.dest     = u16(dst_is_spad ? 1u : 0u); // SpadWriter or DmaWriter
  req.len      = u16(static_cast<std::uint16_t>(local.shape.cols));
  req.block    = u16(static_cast<std::uint16_t>(local.shape.rows));
  req.cmd_id   = u16(issue.rs_tag);
  req.store_en = true;
  dma_req.push(req);

  trace("st_ctrl: dispatched vaddr=0x%llx laddr=0x%x dest=%u len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.dest),
        static_cast<unsigned>(req.len),
        static_cast<unsigned>(req.block),
        static_cast<unsigned>(req.cmd_id));
}

void StCtrl::updateComplete() {
  if (dma_resp.empty() || completed.full()) {
    return;
  }

  const auto response = dma_resp.pop();
  completed.push(static_cast<SmeshRsTag>(response.cmd_id));

  trace("st_ctrl: completed tag=%u", static_cast<unsigned>(response.cmd_id));
}

} // namespace smesh
