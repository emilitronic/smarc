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
  UPDATE(updateWrite).reads(write_in);
}

void Spad::updateWrite() {
  if (write_in.empty()) {
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

  write_accepted_ = true;
  trace("spad: write bank=%u row=%u mask=0x%x cmd_id=%u last=%u",
        static_cast<unsigned>(write.laddr.sp_bank()),
        static_cast<unsigned>(write.laddr.sp_row()),
        static_cast<unsigned>(write.mask),
        static_cast<unsigned>(write.cmd_id),
        static_cast<unsigned>(write.last));
}

void Spad::reset() {
  banks_ = {};
  write_accepted_ = false;
}

const Spad::Row& Spad::row(SmeshLocalAddr addr) const {
  return banks_[addr.sp_bank()][addr.sp_row()];
}

} // namespace smesh
