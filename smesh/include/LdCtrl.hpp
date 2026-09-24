// **********************************************************************
// smesh/include/LdCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026
/*
Load controller declaration.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <array>

namespace smesh {

class LdCtrl : public Component {
  DECLARE_COMPONENT(LdCtrl);

 public:
  LdCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SmeshIssue, cmd_in);      // RS-issued load command to accept
  Output(bit,           completed_val);
  Output(SmeshRsTag,    completed_bits);
  Input(bit,            completed_rdy);
  FifoOutput(DmaReadReq, dma_req);    // DMA read request to memory controller
  FifoInput(DmaReadCompletion, dma_resp); // ack completion of memory move
  void updateCompletionView();
  void updateState();
  void reset();
  // accessor functions for testbench to check LdCtrl state
  bool hasActiveCommand()           const { return (*state_Q_).active_valid; }
  const SmeshIssue& activeCommand() const { return (*state_Q_).active; }
  bool hasDmaResponse()             const { return (*state_Q_).dma_response_valid; }
  std::uint32_t expectedBytes()     const { return (*state_Q_).expected_bytes; }
  std::uint32_t returnedBytes()     const { return (*state_Q_).returned_bytes; }
  SmeshRsTag responseRsTag()        const { return (*state_Q_).response_rs_tag; }

 private:
  struct LoadConfigState {
    std::uint32_t dram_row_stride = 0;
    std::uint32_t ld_block_stride = 0;
  };

  struct State {
    bool active_valid = false;
    SmeshIssue active{};
    bool command_done = false;
    bool dma_response_valid = false;
    bool request_in_flight = false;
    std::uint64_t base_vaddr = 0;
    SmeshLocalAddr base_laddr{};
    std::uint32_t rows = 0;
    std::uint32_t cols = 0;
    std::uint32_t next_row = 0;
    std::uint32_t dram_row_stride = 0;
    std::uint32_t ld_block_stride = 0;
    std::uint32_t expected_bytes = 0;
    std::uint32_t returned_bytes = 0;
    SmeshRsTag response_rs_tag = 0;
    std::array<LoadConfigState, kLoadStates> load_config{};
  };

  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
