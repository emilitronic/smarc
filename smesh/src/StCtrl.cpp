// **********************************************************************
// smesh/src/StCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026

#include "StCtrl.hpp"

#include "SmeshCommand.hpp"

namespace smesh {

StCtrl::StCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateDispatch).reads(cmd_in).writes(dma_req);
}

void StCtrl::updateDispatch() {
  if (cmd_in.empty() || dma_req.full()) {
    return;
  }

  const auto issue = cmd_in.pop();
  const auto local = unpackLocal(static_cast<std::uint64_t>(issue.cmd.rs2));

  DmaWriteReq req{};
  req.vaddr    = issue.cmd.rs1;
  req.laddr    = makeLocalAddr(local.row);
  req.len      = u16(static_cast<std::uint16_t>(local.shape.cols));
  req.block    = u16(static_cast<std::uint16_t>(local.shape.rows));
  req.cmd_id   = u16(issue.rs_tag);
  req.store_en = true;
  dma_req.push(req);

  trace("st_ctrl: dispatched vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.len),
        static_cast<unsigned>(req.block),
        static_cast<unsigned>(req.cmd_id));
}

} // namespace smesh
