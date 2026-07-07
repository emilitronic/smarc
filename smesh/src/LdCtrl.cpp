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
  UPDATE(updateAccept).reads(cmd_in).writes(dma_req);
}

void LdCtrl::updateAccept() {
  if (active_valid_ || cmd_in.empty() || dma_req.full()) {
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
  DmaReadReq req{};
  req.vaddr = active_.cmd.rs1;
  req.laddr = makeLocalAddr(local.row);
  req.cols = u16(static_cast<std::uint16_t>(local.shape.cols));
  req.block_stride = u16(static_cast<std::uint16_t>(kDim));
  req.cmd_id = u16(active_.rob_id);
  dma_req.push(req); // push DMA read request to memory controller

  trace("ld_ctrl: dma request vaddr=0x%llx laddr=0x%x cols=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.cols),
        static_cast<unsigned>(req.cmd_id));
}

void LdCtrl::reset() {
  active_valid_ = false;
  active_ = {};
}

} // namespace smesh
