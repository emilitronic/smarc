// **********************************************************************
// smesh/src/memory/Accum.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 9 2026
/*
Standalone smesh accumulator memory implementation.
*/

#include "Accum.hpp"

namespace smesh {

Accum::Accum(std::string /*name*/, IMPL_CTOR) {
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
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

void Accum::updateWriteReady() {
  const bool completion_blocked = dma_resp.full();
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    write_rdy_bnk[bank] = bit(!completion_blocked);
  }
}

void Accum::updateWrite() {
  bool accepted_write = false;
  bool completion_pushed = false;

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (write_val_bnk[bank] == 0 || write_rdy_bnk[bank] == 0) {
      continue;
    }
    const auto write = *write_bits_bnk[bank];
    assert_always(write.laddr.is_acc_addr(),
                  "Accum write received a scratchpad address");
    assert_always(write.laddr.acc_bank() == bank,
                  "Accum write payload does not match its bank port");

    auto& destination = banks_[bank][write.laddr.acc_row()];
    const auto mask = static_cast<std::uint8_t>(write.mask);
    for (std::size_t lane = 0; lane < kDim; ++lane) {
      if ((mask & (std::uint8_t{1} << lane)) != 0) {
        if (write.has_acc_bitwidth != 0) {
          std::uint32_t word = 0;
          for (std::size_t byte = 0; byte < sizeof(Acc); ++byte) {
            word |= static_cast<std::uint32_t>(write.data[lane * sizeof(Acc) + byte]) << (8 * byte);
          }
          destination[lane] = static_cast<Acc>(word);
        } else {
          const auto byte = static_cast<std::uint8_t>(write.data[lane]);
          destination[lane] = static_cast<Acc>(static_cast<Elem>(byte));
        }
      }
    }

    if (static_cast<bool>(write.last)) {
      assert_always(!completion_pushed,
                    "Accum accepted multiple final DMA writes in one cycle");
      DmaReadCompletion completion{};
      completion.bytes_read = write.bytes_read;
      completion.cmd_id = write.cmd_id;
      dma_resp.push(completion);
      completion_pushed = true;
    }

    accepted_write = true;
    trace("accum: write bank=%u row=%u mask=0x%x cmd_id=%u last=%u",
          static_cast<unsigned>(bank),
          static_cast<unsigned>(write.laddr.acc_row()),
          static_cast<unsigned>(write.mask),
          static_cast<unsigned>(write.cmd_id),
          static_cast<unsigned>(write.last));
  }

  write_accepted_ = write_accepted_ || accepted_write;
}

// A bank can replace a consumed response immediately; a single-port write wins.
void Accum::updateReadReady() {
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    const bool response_pop = read_resp_valid_Q_[bank] == 1 &&
                              read_resp_rdy_bnk[bank] == 1;
    const bool slot_available = read_resp_valid_Q_[bank] == 0 || response_pop;
    const bool write_fire = write_val_bnk[bank] == 1 && write_rdy_bnk[bank] == 1;
    const bool write_blocks_read = kDefaultConfig.acc_singleported && write_fire;
    read_req_rdy_bnk[bank] = bit(slot_available && !write_blocks_read);
  }
}

void Accum::updateReadRespView() {
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    read_resp_val_bnk[bank] = 0;
    read_resp_bits_bnk[bank] = AccumReadResp{};
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (read_resp_valid_Q_[bank] == 1) {
      read_resp_val_bnk[bank] = 1;
      read_resp_bits_bnk[bank] = *read_resp_entry_Q_[bank];
    }
  }
}

void Accum::updateRead() {
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    const bool response_pop = read_resp_valid_Q_[bank] == 1 &&
                              read_resp_rdy_bnk[bank] == 1;
    const bool request_fire = read_req_val_bnk[bank] == 1 &&
                              read_req_rdy_bnk[bank] == 1;

    if (request_fire) {
      const auto req = *read_req_bits_bnk[bank];
      const auto row = static_cast<std::uint32_t>(req.addr) & kAccBankRowMask;
      const auto& source = banks_[bank][row];
      AccumReadResp resp{};
      resp.laddr = makeAccAddr(static_cast<std::uint32_t>(bank * kAccBankRows) + row,
                               false, req.full != 0);
      resp.len = req.len;
      resp.act = req.act;
      resp.scale = req.scale;
      resp.full = req.full;
      resp.cmd_id = req.cmd_id;
      resp.from_dma = req.from_dma;
      resp.data = source;
      for (std::size_t lane = 0; lane < kDim; ++lane) {
        resp.mask |= static_cast<u8>(u8{1} << lane);
      }
      read_resp_entry_D_[bank] = resp;
      read_resp_valid_D_[bank] = 1;
      trace("accum: read bank=%u row=%u mask=0x%x cmd_id=%u",
            static_cast<unsigned>(bank),
            static_cast<unsigned>(row),
            static_cast<unsigned>(resp.mask),
            static_cast<unsigned>(req.cmd_id));
    } else if (response_pop) {
      read_resp_valid_D_[bank] = 0;
      read_resp_entry_D_[bank] = AccumReadResp{};
    }
  }
}

void Accum::reset() {
  banks_ = {};
  write_accepted_ = false;
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    read_resp_valid_Q_[bank].reset(0);
    read_resp_entry_Q_[bank].reset(AccumReadResp{});
    read_resp_valid_D_[bank].reset(0);
    read_resp_entry_D_[bank].reset(AccumReadResp{});
    write_rdy_bnk[bank].reset(1);
    read_req_rdy_bnk[bank].reset(0);
    read_resp_val_bnk[bank].reset(0);
    read_resp_bits_bnk[bank].reset(AccumReadResp{});
  }
}

const Accum::Row& Accum::row(SmeshLocalAddr addr) const {
  return banks_[addr.acc_bank()][addr.acc_row()];
}

} // namespace smesh
