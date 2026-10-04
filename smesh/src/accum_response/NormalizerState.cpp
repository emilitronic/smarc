// **********************************************************************
// smesh/src/accum_response/NormalizerState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 3 2026

#include "NormalizerState.hpp"

namespace smesh {

namespace {

// ****************** Helper Functions ******************
// ******************************************************
// Conversion helpers
// FSM stores state and cmd vals as bytes in its signal bundles, these
// helpers let C++ logic compare them using readable enum names such as
// NormFsmState::Idle and NormCmd::Reset
// converts stored u8 val to NormFsmState enum
NormFsmState stateOf(u8 value) {
  return static_cast<NormFsmState>(static_cast<std::uint8_t>(value));
}
// converts stored u8 val to NormCmd enum
NormCmd commandOf(u8 value) {
  return static_cast<NormCmd>(static_cast<std::uint8_t>(value));
}

// Returns true for certain command/state combos which require the FSM to wait on accepting another packet
// while lane results for those ops are still draining.
bool waitingForLanes(NormFsmState state, NormCmd cmd) {
  return ((cmd == NormCmd::Mean)      && (state == NormFsmState::GetSum || state == NormFsmState::GetMean)) ||
         ((cmd == NormCmd::InvStddev) && (state == NormFsmState::GetSum || state == NormFsmState::GetVariance)) ||
         ((cmd == NormCmd::Max)       && state == NormFsmState::GetMax) ||
         ((cmd == NormCmd::InvSumExp) && state == NormFsmState::GetSum);
}

// Checks if this slot can be treated as finished for accepting another packet.
// Used by updateReady() when decidign whether selected stats slot can accept new packet.
bool operationDone(NormFsmState state, NormCmd cmd, const NormSlotEvents& event, bool output_fire) {
  switch (state) {
    case NormFsmState::Output: // yes if output row is accepted
      return output_fire;
    case NormFsmState::GetMax: // yes if last max chunk is issued for cmd that finishes at that point
      return event.max_last_issued == 1 && cmd == NormCmd::Max;
    case NormFsmState::GetSum: // yes if last sum chunk is issued for cmd that finishes at that point
      return event.sum_last_issued == 1 && (cmd == NormCmd::Sum || cmd == NormCmd::Variance || cmd == NormCmd::SumExp);
    case NormFsmState::WaitingForMean: // yes if mean divide finishes
      return event.divide_finished == 1;
    case NormFsmState::WaitingForScaledInvStddev: // yes if final scaling step finishes for inv std dev
    case NormFsmState::WaitingForScaledInvSumExp: // yes if final scaling step finishes for inv sum of exponentials
      return event.scale_finished == 1;
    default:
      return false;
  }
}

// Decides which state eash stats slot should enter next, based on its current state
// and events from this cycle.  Returns choice, does not update registers itself.
NormFsmState nextState(NormFsmState state, NormCmd cmd, const NormSlotEvents& event, bool output_fire) {
  switch (state) {
    case NormFsmState::Idle:
      return state;
    case NormFsmState::Output:
      return output_fire ? NormFsmState::Idle : state;
    case NormFsmState::GetMax:
      if (event.max_last_issued == 1) { // after last max chunk is issued...
        if (cmd == NormCmd::Max) return NormFsmState::Idle; // ...if cmd was Max, then FSM is done and can return to Idle
        if (cmd == NormCmd::SumExp || cmd == NormCmd::InvSumExp) return NormFsmState::GetSum; // ...if cmd was SumExp or InvSumExp, then FSM needs to get sum of exponentials next
      }
      return state;
    case NormFsmState::GetSum:
      if (event.sum_last_issued == 0) return state; // wait for even that says last sum chunk was processed, until then, stay in GetSum state
      // last sum chunk was issued, now decide next state based on command
      if (cmd == NormCmd::Sum || cmd == NormCmd::Variance || cmd == NormCmd::SumExp) return NormFsmState::Idle;
      if (cmd == NormCmd::Mean)      return NormFsmState::GetMean;
      if (cmd == NormCmd::InvStddev) return NormFsmState::GetVariance;
      if (cmd == NormCmd::InvSumExp) return NormFsmState::GetInvSumExp;
      return state;
    case NormFsmState::GetMean:
      return event.divide_started      == 1 ? NormFsmState::WaitingForMean : state;
    case NormFsmState::WaitingForMean:
      return event.divide_finished     == 1 ? NormFsmState::Idle : state;
    case NormFsmState::GetVariance:
      return event.divide_started      == 1 ? NormFsmState::WaitingForVariance : state;
    case NormFsmState::WaitingForVariance:
      return event.divide_finished     == 1 ? NormFsmState::GetStddev : state;
    case NormFsmState::GetStddev:
      return event.sqrt_started        == 1 ? NormFsmState::WaitingForStddev : state;
    case NormFsmState::WaitingForStddev:
      return event.sqrt_finished       == 1 ? NormFsmState::GetInvStddev : state;
    case NormFsmState::GetInvStddev:
      return event.reciprocal_started  == 1 ? NormFsmState::WaitingForInvStddev : state;
    case NormFsmState::WaitingForInvStddev:
      return event.reciprocal_finished == 1 ? NormFsmState::GetScaledInvStddev : state;
    case NormFsmState::GetScaledInvStddev:
      return event.scale_started       == 1 ? NormFsmState::WaitingForScaledInvStddev : state;
    case NormFsmState::WaitingForScaledInvStddev:
      return event.scale_finished      == 1 ? NormFsmState::Idle : state;
    case NormFsmState::GetInvSumExp:
      return event.exp_divide_started  == 1 ? NormFsmState::WaitingForInvSumExp : state;
    case NormFsmState::WaitingForInvSumExp:
      return event.exp_divide_finished == 1 ? NormFsmState::GetScaledInvSumExp : state;
    case NormFsmState::GetScaledInvSumExp:
      return event.scale_started       == 1 ? NormFsmState::WaitingForScaledInvSumExp : state;
    case NormFsmState::WaitingForScaledInvSumExp:
      return event.scale_finished      == 1 ? NormFsmState::Idle : state;
  }
  return state;
}

} // namespace

// Implementation Constructor
// ******************************************************

NormState::NormState(std::string /*name*/, IMPL_CTOR) {
  // update state at clock edge
  states_Q_ <= states_D_;
  // reads current FSM state and produces a view of them
  UPDATE(updateView).reads(states_Q_).writes(slot_states, out_val, out_id);
  // checks selected stats slot and whether output is draining, outputs tell upstream
  // block whether this packet can be accepted and which stats slot it targets
  UPDATE(updateReady).reads(states_Q_, slot_cmds, events, req_bits, out_val, out_id, out_rdy)
                     .writes(req_rdy, accept_id);
  // checks whether an input packet is accepted and whether any slot's current work advances,
  // produces next state for each slot
  UPDATE(updateNextState).reads(states_Q_, slot_cmds, events, req_val, req_bits, req_rdy, out_val, out_id)
                         .reads(out_rdy)
                         .writes(accept_val, states_D_);
}

void NormState::updateView() {
  const auto states = *states_Q_;
  slot_states       = states; // current FSM state for each slot
  out_val           = 0;
  out_id            = 0;
  // check for slots in Output state (i.e., Normalizer has finished procssing a packet
  // and is holding its result row for downstream block to accept).)
  for (std::size_t id = 0; id < kNormStatsSlots; ++id) {
    if (stateOf(states.state[id]) == NormFsmState::Output) {
      out_val = 1;
      out_id  = u8(id);
      break;
    }
  }
}

// can incoming packed be accepted?  If so, which stats slot is it targeting?
void NormState::updateReady() {
  // read current state and identify requested stats slot
  const auto states         = *states_Q_;
  const auto cmds           = *slot_cmds;
  const auto current_events = *events;
  // use id & output_fire to decide if requested slot can be reused for new packet
  const auto id             = static_cast<std::size_t>(static_cast<std::uint16_t>(req_bits->cmd.stats_id));   // stats slot named by incoming packet
  const bool output_fire    = out_val == 1 && out_rdy == 1; // currently offered result row is being accepted this cycle

  // checks whether any slot is still using the processing lanes
  bool lanes_drained = true; // assume lanes are drained until we find a slot that is still using them
  for (std::size_t slot = 0; slot < kNormStatsSlots; ++slot) {
    lanes_drained &= !waitingForLanes(stateOf(states.state[slot]), commandOf(cmds.cmd[slot]));
  }

  // checks whether incoming packet's target slot is available
  // slot is available if it is Idle or if it is finishing its current op this cycle
  bool slot_available = false;
  if (id < kNormStatsSlots) {
    const auto state            = stateOf(states.state[id]);
    const bool slot_output_fire = output_fire && static_cast<std::uint8_t>(*out_id) == id; // succssful fire must belong to slot being checked
    slot_available              = state == NormFsmState::Idle || operationDone(state, commandOf(cmds.cmd[id]), current_events.slot[id], slot_output_fire);
  }
  // if packets selected stats slot is idle or finishing current op AND processing lanes are drained
  req_rdy   = bit(slot_available && lanes_drained); 
  accept_id = u8(id < kNormStatsSlots ? id : 0); // report requested slot, use slot 0 as fallback if id is out of range (should never happen)
}

void NormState::updateNextState() {
  // snapshot current inputs and intialize next-state candidate
  const auto states         = *states_Q_;
  const auto cmds           = *slot_cmds;
  const auto current_events = *events;
  auto next                 = states; // next starts as copy of current state, will be updated if any slot's state changes
  const bool output_fire    = out_val == 1 && out_rdy == 1;
  const bool accepted       = req_val == 1 && req_rdy == 1; // incoming packet actually handshakes this cycle
  const auto accepted_id    = static_cast<std::size_t>(static_cast<std::uint16_t>(req_bits->cmd.stats_id));
  accept_val                = bit(accepted); // reports whether incoming packet was accepted this cycle
  bool changed              = false; // tracks whether any slot's state changed this cycle, so we can update the register at clock edge

  // compute each slot's orinary next state
  for (std::size_t id = 0; id < kNormStatsSlots; ++id) {
    const auto state            = stateOf(states.state[id]);
    const auto cmd              = commandOf(cmds.cmd[id]);
    const bool slot_output_fire = output_fire && static_cast<std::uint8_t>(*out_id) == id;
    // what state should follow?
    auto next_state             = nextState(state, cmd, current_events.slot[id], slot_output_fire);
    // an accepted new packet can override that slot's ordinary transistion
    if (accepted && accepted_id == id) {
      const auto incoming_cmd = commandOf(req_bits->cmd.cmd);
      // if packet accpeted for the slot its command starts a new op
      // Reset goes to Output, Max goes to GetMax, and other commands begin at GetSum
      // note: Reset is a special case, it goes to Output state so that the saved row 
      //can be sent out before the slot returns to Idle
      next_state = incoming_cmd == NormCmd::Reset ? NormFsmState::Output
                 : incoming_cmd == NormCmd::Max ? NormFsmState::GetMax
                 : NormFsmState::GetSum;
    }
    // record a state change if this slot's next state differs from its current state
    if (next_state != state) {
      next.state[id] = u8(static_cast<std::uint8_t>(next_state));
      changed        = true;
      trace("normalizer_state: slot=%u state=%u next=%u\n",
            static_cast<unsigned>(id), static_cast<unsigned>(state),
            static_cast<unsigned>(next_state));
    }
  }
  if (changed) states_D_ = next; // snapshot written to D register
}

void NormState::reset() {
  states_Q_.reset(NormStateRegs{});
  states_D_.reset(NormStateRegs{});
  slot_states.reset(NormStateRegs{});
  req_rdy.reset(0);
  accept_val.reset(0);
  accept_id.reset(0);
  out_val.reset(0);
  out_id.reset(0);
}

} // namespace smesh
