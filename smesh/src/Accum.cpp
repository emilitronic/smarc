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
  UPDATE(updateWrite).reads(write_in).writes(dma_resp);
  UPDATE(updateReadReady).writes(read_req_rdy);
  UPDATE(updateReadRespView).writes(read_resp_val, read_resp_bits);
  UPDATE(updateReadRespPop).reads(read_resp_rdy);
  UPDATE(updateRead).reads(read_req_val, read_req_bits);
}

void Accum::updateWrite() {
  if (write_in.empty()) {
    return;
  }

  const auto& pending = write_in.peek(); // look at pending write
  // if this is final beat of write & LdCtrl completion FIFO is full, wait
  if (static_cast<bool>(pending.last) && dma_resp.full()) {
    return;
  }

  const auto write = write_in.pop(); // safe to write, so pop it
  assert_always(write.laddr.is_acc_addr(), "Accum write received a scratchpad address");  // check that dest is actual accum addr

  auto& destination = banks_[write.laddr.acc_bank()][write.laddr.acc_row()];  // select accum bank & row
  const auto data = static_cast<std::uint64_t>(write.data);
  const auto mask = static_cast<std::uint8_t>(write.mask);
  for (std::size_t lane = 0; lane < kDim; ++lane) { // for ea. lane (i.e., col of memory row)
    if ((mask & (std::uint8_t{1} << lane)) != 0) {  // if mask bit is set...
      const auto byte = static_cast<std::uint8_t>((data >> (lane * 8)) & 0xffu); // ...copy byte from writ.data
      destination[lane] = static_cast<Acc>(static_cast<Elem>(byte));             // ...to accum row lane (sign-extended to 32 bits)
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
  read_req_rdy = bit(!read_resp_valid_);
}
// shows current response to outside world
void Accum::updateReadRespView() {
  read_resp_val = bit(read_resp_valid_);
  read_resp_bits = read_resp_valid_ ? read_resp_entry_ : AccumReadResp{}; // if  not valid, drive blank response
}
// consumes/clears response when downsream block is ready
void Accum::updateReadRespPop() {
  if (read_resp_valid_ && read_resp_rdy != 0) {
    read_resp_valid_ = false;
    read_resp_entry_ = AccumReadResp{};
  }
}

void Accum::updateRead() {
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
