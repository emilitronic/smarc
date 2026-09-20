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
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    read_resp_valid_Q_[bank] <= read_resp_valid_D_[bank];
    read_resp_entry_Q_[bank] <= read_resp_entry_D_[bank];
  }

  UPDATE(updateWriteReady).reads(dma_resp).writes(write_rdy_bnk);
  UPDATE(updateWrite)
      .reads(write_val_bnk, write_rdy_bnk, write_bits_bnk)
      .writes(dma_resp);
  UPDATE(updateReadReady)
      .reads(read_resp_valid_Q_, read_resp_rdy_bnk,
             write_val_bnk, write_rdy_bnk)
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
  const bool completion_blocked = dma_resp.full();
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    write_rdy_bnk[bank] = bit(!completion_blocked);
  }
}

void Spad::updateWrite() {
  bool accepted_write = false;
  bool completion_pushed = false;

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (write_val_bnk[bank] == 0 || write_rdy_bnk[bank] == 0) {
      continue;
    }
    const auto write = *write_bits_bnk[bank];
    assert_always(!write.laddr.is_acc_addr(),
                  "Spad write received an accumulator address");
    assert_always(write.laddr.sp_bank() == bank,
                  "Spad write payload does not match its bank port");

    auto& destination = banks_[bank][write.laddr.sp_row()];
    const auto data = low64DmaReadData(write.data);
    const auto mask = static_cast<std::uint8_t>(write.mask);
    for (std::size_t lane = 0; lane < kDim; ++lane) {
      if ((mask & (std::uint8_t{1} << lane)) != 0) {
        destination[lane] = static_cast<Elem>((data >> (lane * 8)) & 0xffu);
      }
    }

    // The load path has one response stream, so at most one accepted bank
    // write may complete a DMA command in a cycle.
    if (static_cast<bool>(write.last)) {
      assert_always(!completion_pushed,
                    "Spad accepted multiple final DMA writes in one cycle");
      DmaReadCompletion completion{};
      completion.bytes_read = write.bytes_read;
      completion.cmd_id = write.cmd_id;
      dma_resp.push(completion);
      completion_pushed = true;
    }

    accepted_write = true;
    trace("spad: write bank=%u row=%u mask=0x%x cmd_id=%u last=%u",
          static_cast<unsigned>(bank),
          static_cast<unsigned>(write.laddr.sp_row()),
          static_cast<unsigned>(write.mask),
          static_cast<unsigned>(write.cmd_id),
          static_cast<unsigned>(write.last));
  }

  write_accepted_ = write_accepted_ || accepted_write;
}
// A bank can replace a consumed response immediately; a single-port write wins.
void Spad::updateReadReady() {
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    const bool response_pop = read_resp_valid_Q_[bank] == 1 &&
                              read_resp_rdy_bnk[bank] == 1;
    const bool slot_available = read_resp_valid_Q_[bank] == 0 || response_pop;
    const bool write_fire = write_val_bnk[bank] == 1 && write_rdy_bnk[bank] == 1;
    const bool write_blocks_read = kDefaultConfig.sp_singleported && write_fire;
    read_req_rdy_bnk[bank] = bit(slot_available && !write_blocks_read);
  }
}
// expose current held spad read response onto o/p ports
void Spad::updateReadRespView() {
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    read_resp_val_bnk[bank] = 0;
    read_resp_bits_bnk[bank] = SpadReadResp{};
  }

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (read_resp_valid_Q_[bank] == 1) {
      read_resp_val_bnk[bank] = 1;
      read_resp_bits_bnk[bank] = *read_resp_entry_Q_[bank];
    }
  }
}

void Spad::updateRead() {
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    const bool response_pop = read_resp_valid_Q_[bank] == 1 &&
                              read_resp_rdy_bnk[bank] == 1;
    const bool request_fire = read_req_val_bnk[bank] == 1 &&
                              read_req_rdy_bnk[bank] == 1;

    if (request_fire) {
      const auto req = *read_req_bits_bnk[bank];
      const auto row = static_cast<std::uint32_t>(req.addr) & kSpBankRowMask;
      const auto& source = banks_[bank][row];
      SpadReadResp resp{};
      resp.laddr = makeSpAddr(static_cast<std::uint32_t>(bank * kSpBankRows) + row);
      resp.len = req.len;
      resp.cmd_id = req.cmd_id;
      resp.from_dma = req.from_dma;
      resp.data = source;
      for (std::size_t lane = 0; lane < kDim; ++lane) {
        resp.mask |= static_cast<u8>(u8{1} << lane);
      }
      read_resp_entry_D_[bank] = resp;
      read_resp_valid_D_[bank] = 1;
      trace("spad: read bank=%u row=%u mask=0x%x cmd_id=%u",
            static_cast<unsigned>(bank),
            static_cast<unsigned>(row),
            static_cast<unsigned>(resp.mask),
            static_cast<unsigned>(req.cmd_id));
    } else if (response_pop) {
      read_resp_valid_D_[bank] = 0;
      read_resp_entry_D_[bank] = SpadReadResp{};
    }
  }
}

void Spad::reset() {
  banks_ = {};
  write_accepted_ = false;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    read_resp_valid_Q_[bank].reset(0);
    read_resp_entry_Q_[bank].reset(SpadReadResp{});
    read_resp_valid_D_[bank].reset(0);
    read_resp_entry_D_[bank].reset(SpadReadResp{});
    write_rdy_bnk[bank].reset(1);
    read_req_rdy_bnk[bank].reset(0);
    read_resp_val_bnk[bank].reset(0);
    read_resp_bits_bnk[bank].reset(SpadReadResp{});
  }
}

const Spad::Row& Spad::row(SmeshLocalAddr addr) const {
  return banks_[addr.sp_bank()][addr.sp_row()];
}

void Spad::initializeRow(SmeshLocalAddr addr, const Row& data) {
  assert_always(!addr.is_acc_addr(), "Spad initialization received an accumulator address");
  banks_[addr.sp_bank()][addr.sp_row()] = data;
}

} // namespace smesh
