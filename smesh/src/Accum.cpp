// **********************************************************************
// smesh/src/Accum.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 9 2026
/*
Standalone smesh accumulator memory implementation.
*/

#include "Accum.hpp"

namespace smesh {

Accum::Accum(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateWriteReady).writes(write_rdy_bnk);
  UPDATE(updateWrite).reads(write_in, write_val_bnk, write_bits_bnk).writes(dma_resp);
  UPDATE(updateReadReady).writes(read_req_rdy_bnk);
  UPDATE(updateReadRespView).writes(read_resp_val_bnk, read_resp_bits_bnk);
  UPDATE(updateReadRespPop).reads(read_resp_rdy_bnk);
  UPDATE(updateRead).reads(read_req_val_bnk, read_req_bits_bnk);
}

void Accum::updateWriteReady() {
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    const bool completion_blocked = dma_resp.full();
    write_rdy_bnk[bank] = bit(!completion_blocked);
  }
}

void Accum::updateWrite() {
  bool has_write = false;
  DmaReadResp write{};

  if (!write_in.empty()) {
    const auto& pending = write_in.peek(); // look at pending write
    // if this is final beat of write & LdCtrl completion FIFO is full, wait
    if (static_cast<bool>(pending.last) && dma_resp.full()) {
      return;
    }
    write = write_in.pop(); // safe to write, so pop it
    has_write = true;
  } else {
    for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
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
  }

  if (!has_write) {
    return;
  }

  assert_always(write.laddr.is_acc_addr(), "Accum write received a scratchpad address");  // check that dest is actual accum addr

  auto& destination = banks_[write.laddr.acc_bank()][write.laddr.acc_row()];  // select accum bank & row
  const auto mask = static_cast<std::uint8_t>(write.mask);
  for (std::size_t lane = 0; lane < kDim; ++lane) { // for ea. lane (i.e., col of memory row)
    if ((mask & (std::uint8_t{1} << lane)) != 0) {  // if mask bit is set...
      if (write.has_acc_bitwidth != 0) {
        std::uint32_t word = 0;
        for (std::size_t byte = 0; byte < sizeof(Acc); ++byte) {
          word |= static_cast<std::uint32_t>(write.data[lane * sizeof(Acc) + byte]) << (8 * byte);
        }
        destination[lane] = static_cast<Acc>(word);
      } else {
        const auto byte = static_cast<std::uint8_t>(write.data[lane]);            // ...copy byte from writ.data
        destination[lane] = static_cast<Acc>(static_cast<Elem>(byte));             // ...to accum row lane (sign-extended to 32 bits)
      }
    }
  }
  // if this write is marked last, push completion message to LdCtrl completion FIFO
  if (static_cast<bool>(write.last)) {
    DmaReadCompletion completion{};
    completion.bytes_read = write.bytes_read;
    completion.cmd_id = write.cmd_id;
    dma_resp.push(completion);
  }
  // mark that write happened and emit a trace
  write_accepted_ = true;
  trace("accum: write bank=%u row=%u mask=0x%x cmd_id=%u last=%u",
        static_cast<unsigned>(write.laddr.acc_bank()),
        static_cast<unsigned>(write.laddr.acc_row()),
        static_cast<unsigned>(write.mask),
        static_cast<unsigned>(write.cmd_id),
        static_cast<unsigned>(write.last));
}
// provide read req ready signal to StReadCtrl so it can inspect it
void Accum::updateReadReady() {
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    read_req_rdy_bnk[bank] = bit(!read_resp_valid_);
  }
}
// shows current response to outside world
void Accum::updateReadRespView() {
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    read_resp_val_bnk[bank] = 0;
    read_resp_bits_bnk[bank] = AccumReadResp{};
  }

  if (read_resp_valid_) {
    const auto bank = read_resp_entry_.laddr.acc_bank();
    read_resp_val_bnk[bank] = 1;
    read_resp_bits_bnk[bank] = read_resp_entry_;
  }
}
// consumes/clears response when downsream block is ready
void Accum::updateReadRespPop() {
  if (!read_resp_valid_) {
    return;
  }

  const auto bank = read_resp_entry_.laddr.acc_bank();
  if (read_resp_rdy_bnk[bank] != 0) {
    read_resp_valid_ = false;
    read_resp_entry_ = AccumReadResp{};
  }
}

void Accum::updateRead() {
  const bool exread = false; // TODO: execute read wins once ExCtrl has a local-memory read port
  bool has_request = false;
  AccumReadReq req{};
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (read_req_val_bnk[bank] != 0) {
      req = *read_req_bits_bnk[bank];
      has_request = true;
      break;
    }
  }

  if (!exread && !has_request) {
    return;
  }
  if (exread) {
    return;
  }
  if (read_resp_valid_) {
    return;
  }

  assert_always(req.laddr.is_acc_addr(), "Accum read received a scratchpad address");

  const auto& source = banks_[req.laddr.acc_bank()][req.laddr.acc_row()];
  AccumReadResp resp{};
  resp.laddr = req.laddr;
  resp.len = req.len;
  resp.act = req.act;
  resp.scale = req.scale;
  resp.full = req.full;
  resp.cmd_id = req.cmd_id;
  resp.from_dma = req.from_dma;
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    resp.data |= (static_cast<std::uint64_t>(
                      static_cast<std::uint8_t>(source[lane] & 0xff)) << (lane * 8));
    resp.mask |= static_cast<u8>(u8{1} << lane);
  }
  read_resp_entry_ = resp;
  read_resp_valid_ = true;
  trace("accum: dma read bank=%u row=%u mask=0x%x cmd_id=%u",
        static_cast<unsigned>(req.laddr.acc_bank()),
        static_cast<unsigned>(req.laddr.acc_row()),
        static_cast<unsigned>(resp.mask),
        static_cast<unsigned>(req.cmd_id));
}

void Accum::reset() {
  banks_ = {};
  write_accepted_ = false;
  read_resp_valid_ = false;
  read_resp_entry_ = AccumReadResp{};
}

const Accum::Row& Accum::row(SmeshLocalAddr addr) const {
  return banks_[addr.acc_bank()][addr.acc_row()];
}

} // namespace smesh
