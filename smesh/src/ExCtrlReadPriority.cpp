// **********************************************************************
// smesh/src/ExCtrlReadPriority.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026

#include "ExCtrlReadPriority.hpp"

namespace smesh {

namespace {

// Advance one row phase, wrapping at the active operation's total row count.
std::uint32_t wrappingAddOne(std::uint32_t value, std::uint32_t limit) {
  if (limit == 0) {
    return value;
  }
  const auto next = value + 1;
  return next >= limit ? next - limit : next;
}

// Decide whether two active operand streams contend for the same local-memory
// read resource; inactive, garbage, and im2col-routed operands do not contend.
bool sameBank(const ExCtrlOperand& lhs,
              const ExCtrlOperand& rhs,
              bool im2col_wire,
              bool im2col_en) {
  const bool inactive_or_garbage = lhs.is_garbage != 0 || rhs.is_garbage != 0 ||
                                   lhs.start_inputting == 0 || rhs.start_inputting == 0;
  const bool im2col_suppresses_conflict =
      (lhs.can_be_im2colled != 0 || rhs.can_be_im2colled != 0) &&
      im2col_wire && im2col_en;
  if (inactive_or_garbage || im2col_suppresses_conflict) {
    return false;
  }

  const bool lhs_from_acc = lhs.addr.is_acc_addr();
  const bool rhs_from_acc = rhs.addr.is_acc_addr();
  if (lhs_from_acc && rhs_from_acc) {
    return true;
  }
  return !lhs_from_acc && !rhs_from_acc && lhs.addr.sp_bank() == rhs.addr.sp_bank();
}

// Hold one operand when a higher-priority peer needs the same bank in the same
// phase, or when this operand has advanced one row ahead of its peer.
bool mustWaitFor(const ExCtrlOperand& operand,
                 const ExCtrlOperand& other,
                 std::uint32_t total_rows,
                 bool im2col_wire,
                 bool im2col_en) {
  const bool same_bank = sameBank(operand, other, im2col_wire, im2col_en);  // contending for same bank?
  const bool other_has_higher_priority = other.priority < operand.priority; // compare priority
  const bool same_counter_phase = (operand.started != 0) == (other.started != 0) &&
                                  operand.counter == other.counter; // both ops trying to issue same row phase
  const bool operand_is_one_ahead = operand.started != 0 &&
                                    operand.counter == wrappingAddOne(other.counter, total_rows);
  // Op must wait if it is contending for the same bank with a higher-priority peer in the same phase, or if it has advanced one row ahead of its peer
  return (same_bank && other_has_higher_priority && same_counter_phase) ||
         operand_is_one_ahead;
}

} // namespace

ExCtrlReadPriority::ExCtrlReadPriority(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(a_operand, b_operand, d_operand, total_rows, im2col_wire, im2col_en)
      .writes(a_valid, b_valid, d_valid);
}

void ExCtrlReadPriority::update() {
  // Capture one coherent combinational view of the three operand records.
  const auto a = *a_operand;
  const auto b = *b_operand;
  const auto d = *d_operand;
  const auto rows = static_cast<std::uint32_t>(*total_rows);
  const bool im2col_wire_active = im2col_wire != 0;
  const bool im2col_enabled     = im2col_en != 0;

  // Compare each operand against both peers and combine its reasons to wait.
  const bool a_must_wait = mustWaitFor(a, b, rows, im2col_wire_active, im2col_enabled) ||
                           mustWaitFor(a, d, rows, im2col_wire_active, im2col_enabled);
  const bool b_must_wait = mustWaitFor(b, a, rows, im2col_wire_active, im2col_enabled) ||
                           mustWaitFor(b, d, rows, im2col_wire_active, im2col_enabled);
  const bool d_must_wait = mustWaitFor(d, a, rows, im2col_wire_active, im2col_enabled) ||
                           mustWaitFor(d, b, rows, im2col_wire_active, im2col_enabled);

  // A valid output means arbitration permits that operand to proceed this cycle.
  a_valid = bit(!a_must_wait);
  b_valid = bit(!b_must_wait);
  d_valid = bit(!d_must_wait);
}

} // namespace smesh
