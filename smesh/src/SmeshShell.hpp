// **********************************************************************
// smesh/src/SmeshShell.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshDevice.hpp"
#include "SmeshMemory.hpp"
#include "SmeshPorts.hpp"
#include "SmeshRS.hpp"
#include "smem/MemTypes.hpp"

#include <cstdint>

namespace smesh {

class SmeshShell : public Component {
  DECLARE_COMPONENT(SmeshShell);

 public:
  SmeshShell(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SmeshCmd, cmd_in);
  FifoOutput(SmeshResp, resp_out);
  // native memory master interface
  FifoOutput(smem::MemReq, m_req);
  FifoInput(smem::MemResp, m_resp);

  SmeshMemory& memory() { return memory_; }
  const SmeshMemory& memory() const { return memory_; }
  void setExternalMemory(bool enabled) { external_memory_ = enabled; }

  void update();
  void reset();

 private:
  enum class State {
    Idle,
    MvinIssue,
    MvinWait,
    MvoutIssue,
    MvoutWait,
  };

  struct ActiveMemCmd {
    SmeshFunct funct = SmeshFunct::Flush;
    SmeshRobId rob_id = 0;
    std::uint64_t dram_addr = 0;
    std::uint32_t local_row = 0;
    MatrixShape shape{};
    std::uint32_t stride_bytes = 0;
    std::uint32_t r = 0;
    std::uint32_t c = 0;
    std::uint16_t next_id = 0;
  };

  void startExternalMvin(SmeshFunct funct, std::uint64_t rs1, std::uint64_t rs2, SmeshRobId rob_id);
  void startExternalMvout(std::uint64_t rs1, std::uint64_t rs2, SmeshRobId rob_id);
  void updateExternalMvinIssue();
  void updateExternalMvinWait();
  void updateExternalMvoutIssue();
  void updateExternalMvoutWait();
  void finishActive(std::uint8_t status);

  SmeshRS rs_;
  SmeshDevice device_;
  SmeshMemory memory_;
  bool external_memory_ = false;
  State state_ = State::Idle;
  ActiveMemCmd active_{};
};

} // namespace smesh
