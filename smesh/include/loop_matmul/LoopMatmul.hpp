// **********************************************************************
// smesh/include/loop_matmul/LoopMatmul.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 26 2026
/*
Controller that decodes six LOOP_WS commands into two registered loop slots.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <array>
#include <cstdint>

namespace smesh {

class LoopMatmulCmdQueue;

// One pending LOOP_WS operation, including the fields collected from its setup commands.
struct LoopMatmulSlot {
  std::uint16_t max_i = 0;
  std::uint16_t max_j = 0;
  std::uint16_t max_k = 0;
  std::uint16_t pad_i = 0;
  std::uint16_t pad_j = 0;
  std::uint16_t pad_k = 0;

  std::uint64_t a_dram_addr = 0;
  std::uint64_t b_dram_addr = 0;
  std::uint64_t d_dram_addr = 0;
  std::uint64_t c_dram_addr = 0;
  std::uint64_t a_dram_stride = 0;
  std::uint64_t b_dram_stride = 0;
  std::uint64_t d_dram_stride = 0;
  std::uint64_t c_dram_stride = 0;

  std::uint32_t a_addr_start = 0;
  std::uint32_t b_addr_end = 0;
  std::uint32_t resadd_addr_start = 0;
  std::uint32_t c_spad_addr = 0;
  std::uint8_t  a_ex_spad_id = 0;
  std::uint8_t  b_ex_spad_id = 0;
  std::uint8_t  act = 0;
  bit a_transpose   = false;
  bit b_transpose   = false;
  bit ex_accumulate = false;
  bit full_c        = false;
  bit low_d         = false;
  bit inc_acc_addr  = false;
  bit spad_only     = false;
  bit configured    = false;
  bit running       = false;

  bit lda_started   = false;
  bit ldb_started   = false;
  bit ldd_started   = false;
  bit ex_started    = false;
  bit st_started    = false;
  bit lda_completed = false;
  bit ldb_completed = false;
  bit ldd_completed = false;
  bit ex_completed  = false;
  bit st_completed  = false;
};

// Captures LOOP_WS setup into two slots; command generators will consume them later.
class LoopMatmul : public Component {
  DECLARE_COMPONENT(LoopMatmul);

 public:
  LoopMatmul(std::string name, COMPONENT_CTOR);
  ~LoopMatmul() override;

  Clock(clk);

  Input(bit,            in_val);
  Input(SmeshQueuedCmd, in_bits);
  Output(bit,           in_rdy);

  Output(bit,            out_val);
  Output(SmeshQueuedCmd, out_bits);
  Input(bit,             out_rdy);

  Output(bit,            busy);
  Output(u8,             head_loop_id);
  Output(LoopMatmulSlot, loop0);
  Output(LoopMatmulSlot, loop1);

  void updateDecision();
  void updateSlots();
  void reset();

 private:
  struct State {
    std::array<LoopMatmulSlot, 2> loops{};
    u8  head_id = 0;
    bit is_resadd = false;
  };

  LoopMatmulCmdQueue* cmd_queue_ = nullptr;
  Input(bit, head_val_);
  Input(SmeshQueuedCmd, head_bits_);
  Output(bit, head_rdy_);

  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
