// **********************************************************************
// smesh/src/SmeshShell.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026
/*
Cascade component wrapping the existing SmeshDevice, and SmeshMemory, and SmeshRS.
*/
#include "SmeshShell.hpp"

#include "SmeshCommand.hpp"

#include <exception>

namespace smesh {

SmeshShell::SmeshShell(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cmd_in, m_resp).writes(resp_out, m_req); // native memory master interface
}

void SmeshShell::update() {
  switch (state_) {
    case State::MvinIssue:
      updateExternalMvinIssue();
      return;
    case State::MvinWait:
      updateExternalMvinWait();
      return;
    case State::MvoutIssue:
      updateExternalMvoutIssue();
      return;
    case State::MvoutWait:
      updateExternalMvoutWait();
      return;
    case State::Idle:
      break;
  }

  if (cmd_in.empty() || resp_out.full()) {
    return;
  }

  const auto cmd = cmd_in.peek(); // look at cmd
  const auto cmd_funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(cmd.funct));
  // FLUSH (currently a no-op) handled before RS
  if (cmd_funct == SmeshFunct::Flush) {
    cmd_in.pop();
    SmeshResp resp{};
    try {
      resp.value = static_cast<u64>(device_.executeCustom(memory_,
                                                          cmd_funct,
                                                          static_cast<std::uint64_t>(cmd.rs1),
                                                          static_cast<std::uint64_t>(cmd.rs2)));
      resp.status = 0;
      trace("smesh: system funct=%u ok", static_cast<unsigned>(cmd.funct));
    } catch (const std::exception& e) {
      resp.status = 1;
      resp.value = 0;
      trace("smesh: system funct=%u err=%s", static_cast<unsigned>(cmd.funct), e.what());
    }
    resp_out.push(resp);
    return;
  }

  if (!rs_.canAccept()) {
    return;
  }

  if (!rs_.allocate(cmd)) {       // try to allocate RS entry
    return;
  }
  cmd_in.pop();                   // pop cmd if allocation succeeded
  const auto& entry = rs_.entry();
  SmeshResp resp{};

  try {
    rs_.markIssued(entry.rob_id);
    const auto funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(entry.cmd.funct));
    if (external_memory_ &&
        (funct == SmeshFunct::Mvin || funct == SmeshFunct::Mvin2 || funct == SmeshFunct::Mvin3)) {
      startExternalMvin(funct,
                        static_cast<std::uint64_t>(entry.cmd.rs1),
                        static_cast<std::uint64_t>(entry.cmd.rs2),
                        entry.rob_id);
      return;
    }
    if (external_memory_ && funct == SmeshFunct::Mvout) {
      startExternalMvout(static_cast<std::uint64_t>(entry.cmd.rs1),
                         static_cast<std::uint64_t>(entry.cmd.rs2),
                         entry.rob_id);
      return;
    }
    // execute load/store synchronously if not using external memory, or if the command is not mvin/mvout
    const auto value = device_.executeCustom(memory_,
                                             funct,
                                             static_cast<std::uint64_t>(entry.cmd.rs1),
                                             static_cast<std::uint64_t>(entry.cmd.rs2));
    resp.status = 0;
    resp.value = static_cast<u64>(value);
    trace("smesh: cmd rob=%u funct=%u ok",
          static_cast<unsigned>(entry.rob_id),
          static_cast<unsigned>(entry.cmd.funct));
  } catch (const std::exception& e) {
    resp.status = 1;
    resp.value = 0;
    trace("smesh: cmd rob=%u funct=%u err=%s",
          static_cast<unsigned>(entry.rob_id),
          static_cast<unsigned>(entry.cmd.funct),
          e.what());
  }

  rs_.complete(entry.rob_id);
  resp_out.push(resp);
}

void SmeshShell::reset() {
  device_.reset();
  rs_ = SmeshRS{};
  state_ = State::Idle;
  active_ = {};
}
// mvin when using external_memory_ (rather than memory_)
void SmeshShell::startExternalMvin(SmeshFunct funct, std::uint64_t rs1, std::uint64_t rs2, SmeshRobId rob_id) {
  const auto dst = unpackLocal(rs2);
  std::size_t load_state = 0;
  if (funct == SmeshFunct::Mvin2) {
    load_state = 1;
  } else if (funct == SmeshFunct::Mvin3) {
    load_state = 2;
  }

  active_ = {};
  active_.funct = funct;
  active_.rob_id = rob_id;
  active_.dram_addr = rs1;
  active_.local_row = dst.row;
  active_.shape = dst.shape;
  active_.stride_bytes = device_.state().load_stride_bytes.at(load_state);
  active_.next_id = 0;
  state_ = State::MvinIssue;
  trace("smesh: start external mvin state=%u row=%u rows=%llu cols=%llu",
        static_cast<unsigned>(load_state),
        active_.local_row,
        static_cast<unsigned long long>(active_.shape.rows),
        static_cast<unsigned long long>(active_.shape.cols));
}
// mvout when using external_memory_ (rather than memory_)
void SmeshShell::startExternalMvout(std::uint64_t rs1, std::uint64_t rs2, SmeshRobId rob_id) {
  const auto src = unpackLocal(rs2);
  active_ = {};
  active_.funct = SmeshFunct::Mvout;
  active_.rob_id = rob_id;
  active_.dram_addr = rs1;
  active_.local_row = src.row;
  active_.shape = src.shape;
  active_.stride_bytes = device_.state().store_stride_bytes;
  active_.next_id = 0;
  state_ = State::MvoutIssue;
  trace("smesh: start external mvout row=%u rows=%llu cols=%llu",
        active_.local_row,
        static_cast<unsigned long long>(active_.shape.rows),
        static_cast<unsigned long long>(active_.shape.cols));
}

void SmeshShell::updateExternalMvinIssue() {
  if (active_.r >= active_.shape.rows) {
    finishActive(0);
    return;
  }
  if (m_req.full()) {
    return;
  }

  smem::MemReq req{};
  req.addr = u64(active_.dram_addr + active_.r * active_.stride_bytes + active_.c * sizeof(Elem));
  req.size = u16(sizeof(Elem));
  req.write = false;
  req.id = u16(active_.next_id++);
  m_req.push(req);
  state_ = State::MvinWait;
}

void SmeshShell::updateExternalMvinWait() {
  if (m_resp.empty()) {
    return;
  }

  const auto resp = m_resp.pop();
  if (resp.err != 0) {
    finishActive(1);
    return;
  }

  const auto value = static_cast<Elem>(static_cast<std::uint64_t>(resp.rdata) & 0xffu);
  device_.writeSpadElem(active_.local_row + active_.r, active_.c, value);

  ++active_.c;
  if (active_.c >= active_.shape.cols) {
    active_.c = 0;
    ++active_.r;
  }
  state_ = State::MvinIssue;
}

void SmeshShell::updateExternalMvoutIssue() {
  if (active_.r >= active_.shape.rows) {
    finishActive(0);
    return;
  }
  if (m_req.full()) {
    return;
  }

  smem::MemReq req{};
  const auto value = device_.readAccElem(active_.local_row + active_.r, active_.c);
  req.addr = u64(active_.dram_addr + active_.r * active_.stride_bytes + active_.c * sizeof(Acc));
  req.size = u16(sizeof(Acc));
  req.write = true;
  req.wdata = u64(static_cast<std::uint32_t>(value));
  req.id = u16(active_.next_id++);
  m_req.push(req);
  state_ = State::MvoutWait;
}

void SmeshShell::updateExternalMvoutWait() {
  if (m_resp.empty()) {
    return;
  }

  const auto resp = m_resp.pop();
  if (resp.err != 0) {
    finishActive(1);
    return;
  }

  ++active_.c;
  if (active_.c >= active_.shape.cols) {
    active_.c = 0;
    ++active_.r;
  }
  state_ = State::MvoutIssue;
}

void SmeshShell::finishActive(std::uint8_t status) {
  if (resp_out.full()) {
    return;
  }
  SmeshResp resp{};
  resp.status = u8(status);
  resp.value = 0;
  resp_out.push(resp);
  rs_.complete(active_.rob_id);
  state_ = State::Idle;
  active_ = {};
}

} // namespace smesh
