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
  read_resp_valid_Q_ <= read_resp_valid_D_;
  read_resp_entry_Q_ <= read_resp_entry_D_;

  UPDATE(updateWriteReady).writes(write_rdy_bnk);
  UPDATE(updateWrite).reads(write_val_bnk, write_bits_bnk).writes(dma_resp);
  UPDATE(updateReadReady)
      .reads(read_req_val_bnk, read_resp_valid_Q_)
      .writes(read_req_rdy_bnk);
  UPDATE(updateReadRespView)
      .reads(read_resp_valid_Q_, read_resp_entry_Q_)
      .writes(read_resp_val_bnk, read_resp_bits_bnk);
  UPDATE(updateRead)
      .reads(read_req_val_bnk, read_req_bits_bnk, read_req_rdy_bnk,
             read_resp_valid_Q_, read_resp_entry_Q_, read_resp_rdy_bnk)
      .writes(read_resp_valid_D_, read_resp_entry_D_);
}

void Spad::updateWriteReady() {
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    const bool completion_blocked = dma_resp.full();
    write_rdy_bnk[bank] = bit(!completion_blocked);
  }
}

void Spad::updateWrite() {
  bool has_write = false;
  DmaReadResp write{};

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (write_val_bnk[bank] == 0) {
      continue;
    }
    const auto pending = *write_bits_bnk[bank];
    if (static_cast<bool>(pending.last) && dma_resp.full()) {
      return;
    }
    write = pending;
    has_write = true;
    break;
  }

  if (!has_write) {
    return;
  }

  assert_always(!write.laddr.is_acc_addr(),
                "Spad write received an accumulator address");

  auto& destination = banks_[write.laddr.sp_bank()][write.laddr.sp_row()];
  const auto data = low64DmaReadData(write.data);
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
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    read_req_rdy_bnk[bank] = 0;
  }

  if (*read_resp_valid_Q_ == 1) {
    return;
  }

  // Spad has one read port, so only the first valid bank can handshake.
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (read_req_val_bnk[bank] == 1) {
      read_req_rdy_bnk[bank] = 1;
      break;
    }
  }
}
// expose current held spad read response onto o/p ports
void Spad::updateReadRespView() {
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    read_resp_val_bnk[bank] = 0;
    read_resp_bits_bnk[bank] = SpadReadResp{};
  }

  if (*read_resp_valid_Q_ == 1) {
    const auto resp = *read_resp_entry_Q_;
    const auto bank = resp.laddr.sp_bank();
    read_resp_val_bnk[bank] = 1;
    read_resp_bits_bnk[bank] = resp;
  }
}

void Spad::updateRead() {
  auto next_valid = *read_resp_valid_Q_;
  auto next_entry = *read_resp_entry_Q_;

  // Consume the held response. A new request may be accepted next cycle.
  if (*read_resp_valid_Q_ == 1) {
    const auto response_bank = read_resp_entry_Q_->laddr.sp_bank();
    if (read_resp_rdy_bnk[response_bank] == 1) {
      next_valid = 0;
      next_entry = SpadReadResp{};
    }
  }

  // Accept exactly one request, and only when its ready/valid handshake occurs.
  if (*read_resp_valid_Q_ == 0) {
    bool has_request = false;
    SpadReadReq req{};
    for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
      if (read_req_val_bnk[bank] == 1 && read_req_rdy_bnk[bank] == 1) {
        req = *read_req_bits_bnk[bank];
        has_request = true;
        break;
      }
    }

    if (has_request) {
      assert_always(!req.laddr.is_acc_addr(),
                    "Spad read received an accumulator address");

      const auto& source = banks_[req.laddr.sp_bank()][req.laddr.sp_row()];
      SpadReadResp resp{};
      resp.laddr = req.laddr;
      resp.len = req.len;
      resp.cmd_id = req.cmd_id;
      resp.from_dma = req.from_dma;
      resp.data = source;
      for (std::size_t lane = 0; lane < kDim; ++lane) {
        resp.mask |= static_cast<u8>(u8{1} << lane);
      }
      next_entry = resp;
      next_valid = 1;
      trace("spad: read bank=%u row=%u mask=0x%x cmd_id=%u",
            static_cast<unsigned>(req.laddr.sp_bank()),
            static_cast<unsigned>(req.laddr.sp_row()),
            static_cast<unsigned>(resp.mask),
            static_cast<unsigned>(req.cmd_id));
    }
  }

  read_resp_valid_D_ = next_valid;
  read_resp_entry_D_ = next_entry;
}

void Spad::reset() {
  banks_ = {};
  write_accepted_ = false;
  read_resp_valid_Q_.reset(0);
  read_resp_entry_Q_.reset(SpadReadResp{});
  read_resp_valid_D_.reset(0);
  read_resp_entry_D_.reset(SpadReadResp{});
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    write_rdy_bnk[bank].reset(1);
    read_req_rdy_bnk[bank].reset(0);
    read_resp_val_bnk[bank].reset(0);
    read_resp_bits_bnk[bank].reset(SpadReadResp{});
  }
}

const Spad::Row& Spad::row(SmeshLocalAddr addr) const {
  return banks_[addr.sp_bank()][addr.sp_row()];
}

} // namespace smesh
