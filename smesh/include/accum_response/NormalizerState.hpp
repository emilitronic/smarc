// **********************************************************************
// smesh/include/accum_response/NormalizerState.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 3 2026

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace smesh {

constexpr std::size_t kNormStatsSlots = 2; // two stat slots

// operations carried in input packet
enum class NormCmd : std::uint8_t {
  Reset,      // 
  Sum,
  Mean,
  Variance,
  InvStddev,
  Max,
  SumExp,
  InvSumExp
};

// FSM progress through the operations for each slot
enum class NormFsmState : std::uint8_t {
  Idle,
  Output,                    // saved row is ready to leave
  GetSum,                    // row chunks being sent to sum lanes
  GetMean,
  WaitingForMean,            // divide has started, FSM waits for result
  GetVariance,
  WaitingForVariance,
  GetStddev,
  WaitingForStddev,
  GetInvStddev,
  WaitingForInvStddev,
  GetScaledInvStddev,
  WaitingForScaledInvStddev, 
  GetMax,
  GetInvSumExp,
  WaitingForInvSumExp,
  GetScaledInvSumExp,
  WaitingForScaledInvSumExp
};

struct NormStateRegs {
  std::array<u8, kNormStatsSlots> state{}; // holds FSM state fore each of two slots
};

struct NormSlotCmds {
  std::array<u8, kNormStatsSlots> cmd{}; // holds command for each of two slots
};

// Carries progress signals for one slot
struct NormSlotEvents {
  bit sum_last_issued     = 0; // last sum chunk was issued
  bit max_last_issued     = 0;
  bit divide_started      = 0;
  bit divide_finished     = 0;
  bit sqrt_started        = 0;
  bit sqrt_finished       = 0;
  bit reciprocal_started  = 0;
  bit reciprocal_finished = 0;
  bit scale_started       = 0;
  bit scale_finished      = 0;
  bit exp_divide_started  = 0;
  bit exp_divide_finished = 0;
};

// Contains one set of progress signals per slot
struct NormStateEvents {
  std::array<NormSlotEvents, kNormStatsSlots> slot{};
};

// Controls one FSM per statistics slot. Row and statistic registers live elsewhere.
class NormState : public Component {
  DECLARE_COMPONENT(NormState);

 public:
  NormState(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,             req_val);
  Input(AccNormReq,      req_bits);   // incoming packet
  Input(NormSlotCmds,    slot_cmds);  // saved cmd for each slot
  Input(NormStateEvents, events);     // progress sigs from lane and math blocks
  Input(bit,             out_rdy);

  Output(bit,           req_rdy);
  Output(bit,           accept_val); 
  Output(u8,            accept_id);   // which slot accepted the input packet
  Output(bit,           out_val);
  Output(u8,            out_id);
  Output(NormStateRegs, slot_states); // current FSM state for each slot

  void updateView();
  void updateReady();
  void updateNextState();
  void reset() override;

 private:
  Output(NormStateRegs,   states_Q_);
  Register(NormStateRegs, states_D_);
};

} // namespace smesh
