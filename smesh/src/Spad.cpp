// **********************************************************************
// smesh/src/Spad.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Standalone smesh scratchpad memory implementation.
*/

#include "Spad.hpp"

namespace smesh {

Spad::Spad(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateWrite).reads(write_in).writes(dma_resp);
  UPDATE(updateReadReady).writes(read_req_rdy, read_req_rdy_banked);
  UPDATE(updateReadRespView).writes(read_resp_val, read_resp_bits);
  UPDATE(updateReadRespPop).reads(read_resp_rdy);
  UPDATE(updateRead).reads(read_req_val, read_req_bits);
}

void Spad::updateWrite() {
  if (write_in.empty()) {
    return;
  }
  // wait until completion FIFO to LdCtrl has room before performing final write
  const auto& pending = write_in.peek();
  if (static_cast<bool>(pending.last) && dma_resp.full()) {
    return;
  }

  const auto write = write_in.pop();
  assert_always(!write.laddr.is_acc_addr(),
                "Spad write received an accumulator address");

  auto& destination = banks_[write.laddr.sp_bank()][write.laddr.sp_row()];
  const auto data = static_cast<std::uint64_t>(write.data);
  const auto mask = static_cast<std::uint8_t>(write.mask);
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    if ((mask & (std::uint8_t{1} << lane)) != 0) {
      destination[lane] = static_cast<Elem>((data >> (lane * 8)) & 0xffu);
    }
  }
  // if this is final write push {bytes_read, cmd_id} on completion FIFO to LdCtrl
  if (static_cast<bool>(write.last)) {
    DmaReadCompletion completion{};
    completion.bytes_read = write.bytes_read;
    completion.cmd_id = write.cmd_id;
    dma_resp.push(completion);
  }

  write_accepted_ = true;
  trace("spad: write bank=%u row=%u mask=0x%x cmd_id=%u last=%u",
        static_cast<unsigned>(write.laddr.sp_bank()),
        static_cast<unsigned>(write.laddr.sp_row()),
        static_cast<unsigned>(write.mask),
        static_cast<unsigned>(write.cmd_id),
        static_cast<unsigned>(write.last));
}
// provide read req ready signal to StReadCtrl so it can inspect it
void Spad::updateReadReady() {
  read_req_rdy = bit(!read_resp_valid_);
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    read_req_rdy_banked[bank] = bit(!read_resp_valid_);
  }
}

void Spad::updateReadRespView() {
  read_resp_val = bit(read_resp_valid_);
  read_resp_bits = read_resp_valid_ ? read_resp_entry_ : SpadReadResp{};
}

void Spad::updateReadRespPop() {
  if (read_resp_valid_ && read_resp_rdy != 0) {
    read_resp_valid_ = false;
    read_resp_entry_ = SpadReadResp{};
  }
}

void Spad::updateRead() {
  const bool exread = false; // TODO: execute read wins once ExCtrl has a local-memory read port
  const bool dmawrite = read_req_val != 0;
  if (!exread && !dmawrite) {
    return;
  }
  if (exread) {
    return;
  }
  if (read_resp_valid_) {
    return;
  }

  const auto req = *read_req_bits;
  assert_always(!req.laddr.is_acc_addr(),
                "Spad read received an accumulator address");

  const auto& source = banks_[req.laddr.sp_bank()][req.laddr.sp_row()];
  SpadReadResp resp{};
  resp.laddr = req.laddr;
  resp.len = req.len;
  resp.cmd_id = req.cmd_id;
  resp.from_dma = req.from_dma;
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    resp.data |= (static_cast<std::uint64_t>(
                      static_cast<std::uint8_t>(source[lane])) << (lane * 8));
    resp.mask |= static_cast<u8>(u8{1} << lane);
  }
  read_resp_entry_ = resp;
  read_resp_valid_ = true;
  trace("spad: dma read bank=%u row=%u mask=0x%x cmd_id=%u",
        static_cast<unsigned>(req.laddr.sp_bank()),
        static_cast<unsigned>(req.laddr.sp_row()),
        static_cast<unsigned>(resp.mask),
        static_cast<unsigned>(req.cmd_id));
}

void Spad::reset() {
  banks_ = {};
  write_accepted_ = false;
  read_resp_valid_ = false;
  read_resp_entry_ = SpadReadResp{};
}

const Spad::Row& Spad::row(SmeshLocalAddr addr) const {
  return banks_[addr.sp_bank()][addr.sp_row()];
}

} // namespace smesh
