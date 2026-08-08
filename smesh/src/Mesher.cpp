// **********************************************************************
// smesh/src/Mesher.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026

#include "Mesher.hpp"

namespace smesh {

namespace {

std::uint8_t wrappingAdd(std::uint8_t value, std::uint8_t addend, std::uint8_t limit) {
  const auto next = static_cast<std::uint8_t>(value + addend);
  return next >= limit ? static_cast<std::uint8_t>(next - limit) : next;
}

} // namespace

Mesher::Mesher(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(req_val,
             req_bits)
      .writes(req_rdy);
}

void Mesher::update() {
  const auto cur_req_state       = req_state_;
  const bool cur_req_state_valid = req_state_valid_;
  const auto cur_matmul_id       = matmul_id_;
  const auto cur_next_matmul_id  = next_matmul_id_;
  const bool cur_in_prop         = in_prop_;
  const bool cur_a_written       = a_written_;
  const bool cur_b_written       = b_written_;
  const bool cur_d_written       = d_written_;
  const auto cur_fire_counter    = fire_counter_;

  const bool last_fire = false; // TODO: compute from row/input-advance completion logic.

  req_rdy = bit(!cur_req_state_valid || last_fire);

  const bool req_fire = req_val != 0 && req_rdy != 0;
  (void) req_fire;

  auto next_req_state       = cur_req_state;
  bool next_req_state_valid = cur_req_state_valid;
  auto next_matmul_id       = cur_matmul_id;
  auto next_next_matmul_id  = cur_next_matmul_id;
  bool next_in_prop         = cur_in_prop;
  bool next_a_written       = cur_a_written;
  bool next_b_written       = cur_b_written;
  bool next_d_written       = cur_d_written;
  auto next_fire_counter    = cur_fire_counter;

  if (req_fire) {
    next_req_state       = *req_bits; // push in new req
    next_req_state_valid = true;      // mark as valid
    next_in_prop         = (req_bits->pe_control.propagate != 0) != cur_in_prop;
    next_matmul_id       = wrappingAdd(cur_matmul_id, 1, static_cast<std::uint8_t>(kMaxSimultaneousMatmuls));
  } else if (last_fire) {
    next_req_state_valid = cur_req_state.flush > 1;
    next_req_state.flush = static_cast<u8>(cur_req_state.flush - 1);
  }

  req_state_       = next_req_state;
  req_state_valid_ = next_req_state_valid;
  matmul_id_       = next_matmul_id;
  next_matmul_id_  = next_next_matmul_id;
  in_prop_         = next_in_prop;
  a_written_       = next_a_written;
  b_written_       = next_b_written;
  d_written_       = next_d_written;
  fire_counter_    = next_fire_counter;
}

void Mesher::reset() {
  req_state_       = ExCtrlMeshReq{};
  req_state_valid_ = false;
  matmul_id_       = 0;
  next_matmul_id_  = 0;
  in_prop_         = false;
  a_written_       = false;
  b_written_       = false;
  d_written_       = false;
  fire_counter_    = 0;

  req_rdy.reset(0);
}

} // namespace smesh
