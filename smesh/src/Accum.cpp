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

void Accum::reset() {
  banks_ = {};
  write_accepted_ = false;
}

const Accum::Row& Accum::row(SmeshLocalAddr addr) const {
  return banks_[addr.acc_bank()][addr.acc_row()];
}

} // namespace smesh
