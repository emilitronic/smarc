// **********************************************************************
// smesh/src/SmeshShell.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026
/*
Cascade component wrapping the existing SmeshDevice.
*/
#include "SmeshShell.hpp"

#include "SmeshCommand.hpp"

#include <exception>

namespace smesh {

SmeshShell::SmeshShell(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cmd_in).writes(resp_out);
}

void SmeshShell::update() {
  if (cmd_in.empty() || resp_out.full()) {
    return;
  }

  const auto cmd = cmd_in.pop();
  SmeshResp resp{};

  try {
    const auto value = device_.executeCustom(
        memory_,
        static_cast<SmeshFunct>(static_cast<std::uint32_t>(cmd.funct)),
        static_cast<std::uint64_t>(cmd.rs1),
        static_cast<std::uint64_t>(cmd.rs2));
    resp.status = 0;
    resp.value = static_cast<u64>(value);
    trace("smesh: cmd funct=%u ok", static_cast<unsigned>(cmd.funct));
  } catch (const std::exception& e) {
    resp.status = 1;
    resp.value = 0;
    trace("smesh: cmd funct=%u err=%s", static_cast<unsigned>(cmd.funct), e.what());
  }

  resp_out.push(resp);
}

void SmeshShell::reset() {
  device_.reset();
}

} // namespace smesh
