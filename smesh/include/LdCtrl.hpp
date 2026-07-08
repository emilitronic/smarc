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
  FifoOutput(SmeshRobId, completed);  // Let RS know when load is done (rob_id)
  FifoOutput(DmaReadReq, dma_req);    // DMA read request to memory controller
  FifoInput(DmaReadCompletion, dma_resp); // ack completion of memory move
  // per-cycle update functions
  void updateAccept();      // how to accept load command
  void updateIssue();       // how to issue next row of active load command to DMA
  void updateDmaResponse(); // how to handle memory completion ack
  void updateComplete();    // how to report completed command to RS
  void reset();
  // accessor functions for testbench to check LdCtrl state
  bool hasActiveCommand()           const { return active_valid_; }
  const SmeshIssue& activeCommand() const { return active_; }
  bool hasDmaResponse()             const { return dma_response_valid_; }
  std::uint32_t expectedBytes()     const { return expected_bytes_; }
  std::uint32_t returnedBytes()     const { return returned_bytes_; }
  SmeshRobId responseCommandId()    const { return response_cmd_id_; }

 private:
  struct LoadConfigState {
    std::uint32_t dram_row_stride = 0;
    std::uint32_t ld_block_stride = 0;
  };

  bool active_valid_        = false;  // whether LdCtrl has active command from RS
  SmeshIssue active_{};               // active command and its rob_id from RS
  bool command_done_        = false;
  bool dma_response_valid_  = false;  // has a DMA completion response returned
  bool request_in_flight_   = false;  // one DMA row request is outstanding
  std::uint64_t base_vaddr_ = 0;
  SmeshLocalAddr base_laddr_{};
  std::uint32_t rows_ = 0;
  std::uint32_t cols_ = 0;
  std::uint32_t next_row_        = 0; // next row to issue to DMA
  std::uint32_t dram_row_stride_ = 0; // stride in bytes between rows in DRAM
  std::uint32_t ld_block_stride_ = 0; // stride in local rows between blocks of rows in local memory
  std::uint32_t expected_bytes_  = 0; // total bytes expected for active command
  std::uint32_t returned_bytes_  = 0; // total bytes returned for active command (accumulated across multiple DMA responses)
  SmeshRobId response_cmd_id_    = 0; // commmand ID from most recent DMA completion response (should match active_.rob_id)
  std::array<LoadConfigState, kLoadStates> load_config_{};
};

} // namespace smesh
