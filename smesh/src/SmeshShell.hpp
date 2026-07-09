// **********************************************************************
// smesh/src/SmeshShell.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026
/*
What the design consists of.
*/
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
  ~SmeshShell() override;

  // ********** HARDWARE PORTS **********

  Clock(clk);

  // cmd driver interface
  FifoInput(SmeshCmd, cmd_in);
  FifoOutput(SmeshResp, resp_out);
  // memory interface
  FifoOutput(smem::MemReq, m_req);
  FifoInput(smem::MemResp, m_resp);
  // RS allocation interface
  FifoOutput(SmeshQueuedCmd, rs_alloc_out);

  // ********** TESTBENCH / SIMULATION CONTROLS **********
  // internal fake memory for simple sims
  SmeshMemory&       memory() { return memory_; } // mutable version
  const SmeshMemory& memory() const { return memory_; }
  // external memory mode for more realistic sims
  void setExternalMemory(bool enabled) { external_memory_ = enabled; }

  // ********** CLOCKED BEHAVIOR **********

  void update();
  void reset();

 private:
  // ********** INTERNAL CONTROL TYPES **********

  enum class State {
    Idle,
    MvinIssue,
    MvinWait,
    MvoutIssue,
    MvoutWait,
  };

  struct ActiveMemCmd {
    SmeshFunct    funct = SmeshFunct::Flush;
    SmeshRsTag    rs_tag = 0;
    std::uint64_t dram_addr = 0;
    std::uint32_t local_row = 0;
    MatrixShape   shape{};
    std::uint32_t stride_bytes = 0;
    std::uint32_t r = 0;
    std::uint32_t c = 0;
    std::uint16_t next_id = 0;
  };

  // ********** MEMORY SEQUENCER BEHAVIOR **********

  void startExternalMvin(SmeshFunct funct, std::uint64_t rs1, std::uint64_t rs2, SmeshRsTag rs_tag);
  void updateExternalMvinIssue();
  void updateExternalMvinWait();
  void startExternalMvout(std::uint64_t rs1, std::uint64_t rs2, SmeshRsTag rs_tag);
  void updateExternalMvoutIssue();
  void updateExternalMvoutWait();
  void finishActive(std::uint8_t status);

  // ********** INTERNAL BLOCKS AND STATE **********

  SmeshDevice  device_;       // compute, spad, and accum
  SmeshRS*     rs_ = nullptr;
  ActiveMemCmd active_{};     // cmd data being processed
  State state_ = State::Idle; // control phase state

  SmeshMemory memory_; // internal mem for simple sims
  bool external_memory_ = false; // use external mem
};

} // namespace smesh
