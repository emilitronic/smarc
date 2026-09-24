// **********************************************************************
// smesh/include/LdCtrlState.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshTypes.hpp"

#include <array>
#include <cstdint>

namespace smesh {

enum class LdCtrlFsmState : std::uint8_t {
  WaitingForCommand,
  WaitingForDmaReqReady,
  SendingRows
};

struct LdCtrlStateRegs {
  std::uint8_t state = static_cast<std::uint8_t>(LdCtrlFsmState::WaitingForCommand);
  std::uint32_t row_counter = 0;
  std::array<std::uint64_t, kLoadStates> strides{};
  std::array<std::uint32_t, kLoadStates> scales{};
  std::array<bit, kLoadStates> shrinks{};
  std::array<std::uint16_t, kLoadStates> block_strides{};
  std::array<std::uint8_t, kLoadStates> pixel_repeats{};
  // Config values are unspecified until the corresponding slot is written.
  std::array<bit, kLoadStates> configured{};
};

// Load FSM register bank. Only CONFIG acceptance is implemented so far.
class LdCtrlState : public Component {
  DECLARE_COMPONENT(LdCtrlState);

 public:
  LdCtrlState(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, head_val);
  Input(bit, do_config);
  Input(u8,  config_state_id);
  Input(u64, config_stride);
  Input(u32, config_scale);
  Input(bit, config_shrink);
  Input(u16, config_block_stride);
  Input(u8,  config_pixel_repeats);

  Output(bit,      head_rdy);
  Output(u8,       control_state);
  Output(u32,      row_counter);
  OutputArray(bit, configured, kLoadStates);
  OutputArray(u64, strides, kLoadStates);
  OutputArray(u32, scales, kLoadStates);
  OutputArray(bit, shrinks, kLoadStates);
  OutputArray(u16, block_strides, kLoadStates);
  OutputArray(u8,  pixel_repeats, kLoadStates);

  void updateView();
  void updateConfig();
  void reset();

 private:
  Output(LdCtrlStateRegs, regs_Q_);
  Register(LdCtrlStateRegs, regs_D_);
};

} // namespace smesh
