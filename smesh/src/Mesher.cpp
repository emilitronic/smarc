// **********************************************************************
// smesh/src/Mesher.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026

#include "Mesher.hpp"

#include <stdexcept>

namespace smesh {

namespace {
// wrapping addition for mesh-local matmul IDs and queue indices
std::uint8_t wrappingAdd(std::uint8_t value, std::uint8_t addend, std::uint8_t limit) {
  const auto next = static_cast<std::uint8_t>(value + addend);
  return next >= limit ? static_cast<std::uint8_t>(next - limit) : next;
}

std::uint32_t wrappingAdd(std::uint32_t value, std::uint32_t addend, std::uint32_t limit) {
  const auto next = value + addend;
  return next >= limit ? next - limit : next;
}

MesherTag makeGarbageTag() {
  MesherTag tag{};
  tag.addr = SmeshLocalAddr{
      kLocalAddrIsAccMask |
      kLocalAddrAccumulateMask |
      kLocalAddrReadFullAccRowMask |
      kLocalAddrGarbageMask |
      kLocalAddrDataMask};
  return tag;
}

} // namespace

Mesher::Mesher(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReady)
      .writes(req_rdy, a_rdy, b_rdy, d_rdy);

  UPDATE(updateTagsInProgress)
      .writes(tags_in_progress);

  UPDATE(update)
      .reads(req_val, req_bits,
             a_val, a_bits,
             b_val, b_bits,
             d_val, d_bits)
      .reads(transposer_out_col_bits)
      .writes(transposer_in_row_val, transposer_in_row_bits,
              resp_val, resp_bits);
}

void Mesher::updateReady() {
  const bool input_next_row_into_spatial_array =
      req_state_valid_ &&
      ((a_written_ && b_written_ && d_written_) || req_state_.flush > 0);
  const bool last_fire =
      input_next_row_into_spatial_array &&
      fire_counter_ == req_state_.total_rows - 1;

  req_rdy = bit(!req_state_valid_ || last_fire);
  a_rdy   = bit(!a_written_ || input_next_row_into_spatial_array || req_rdy != 0);
  b_rdy   = bit(!b_written_ || input_next_row_into_spatial_array || req_rdy != 0);
  d_rdy   = bit(!d_written_ || input_next_row_into_spatial_array || req_rdy != 0);
}

void Mesher::updateTagsInProgress() {
  // TODO: expose occupied tagq entries once in-flight tag reporting is implemented.
  for (std::size_t i = 0; i < kMesherTagQueueEntries; ++i) {
    tags_in_progress[i] = MesherTag{};
  }
}

void Mesher::update() {
  const auto cur_req_state          = req_state_;
  const bool cur_req_state_valid    = req_state_valid_;
  const auto cur_matmul_id          = matmul_id_;
  const auto cur_next_matmul_id     = next_matmul_id_;
  const bool cur_in_prop            = in_prop_;
  const bool cur_a_written          = a_written_;
  const bool cur_b_written          = b_written_;
  const bool cur_d_written          = d_written_;
  const auto cur_fire_counter       = fire_counter_;
  const auto cur_tagq_head          = tagq_head_;
  const auto cur_tagq_tail          = tagq_tail_;
  const auto cur_tagq_count         = tagq_count_;
  const auto cur_total_rows_q_head  = total_rows_q_head_;
  const auto cur_total_rows_q_tail  = total_rows_q_tail_;
  const auto cur_total_rows_q_count = total_rows_q_count_;

  // *******************
  // Combinational Logic
  // *******************
  const bool input_next_row_into_spatial_array = cur_req_state_valid && ((cur_a_written && cur_b_written && cur_d_written) || cur_req_state.flush > 0);
  const bool pause = !cur_req_state_valid || !input_next_row_into_spatial_array;
  const auto total_fires = cur_req_state.total_rows;
  // note: keep input_next_row_into_spatial_array first so C++ does not evaluate total_fires - 1 when no request is active.
  const bool last_fire = input_next_row_into_spatial_array && cur_fire_counter == total_fires - 1;

  const bool req_ready = !cur_req_state_valid || last_fire;
  const bool a_ready   = !cur_a_written || input_next_row_into_spatial_array || req_ready;
  const bool b_ready   = !cur_b_written || input_next_row_into_spatial_array || req_ready;
  const bool d_ready   = !cur_d_written || input_next_row_into_spatial_array || req_ready;

  const bool req_fire = req_val != 0 && req_ready;
  const bool a_fire   = a_val != 0 && a_ready;
  const bool b_fire   = b_val != 0 && b_ready;
  const bool d_fire   = d_val != 0 && d_ready;
  const bool dataflow_os = cur_req_state.pe_control.dataflow == kExDataflowOS;
  const bool dataflow_ws = cur_req_state.pe_control.dataflow == kExDataflowWS;
  const bool a_from_transposer = dataflow_os ? cur_req_state.a_transpose == 0 : cur_req_state.a_transpose != 0;
  const bool b_from_transposer = dataflow_os && cur_req_state.bd_transpose != 0;
  const bool d_from_transposer = dataflow_ws && cur_req_state.bd_transpose != 0;
  const int transposer_sources =
      (a_from_transposer ? 1 : 0) +
      (b_from_transposer ? 1 : 0) +
      (d_from_transposer ? 1 : 0);
  if (transposer_sources > 1) {
    throw std::logic_error("Mesher: multiple operands selected for the single transposer");
  }

  MeshInputRow transposer_in{};
  if (a_from_transposer) {
    transposer_in = a_bits->data;
  } else if (b_from_transposer) {
    transposer_in = b_bits->data;
  } else if (d_from_transposer) {
    transposer_in = d_bits->data;
  }
  transposer_in_row_val  = bit(!pause && transposer_sources != 0);
  transposer_in_row_bits = transposer_in;

  MeshHullIn hull_in{};
  hull_in.a_fire     = bit(a_fire);
  hull_in.a_bits     = a_bits->data;
  hull_in.b_fire     = bit(b_fire);
  hull_in.b_bits     = b_bits->data;
  hull_in.d_fire     = bit(d_fire);
  hull_in.d_bits     = d_bits->data;
  hull_in.transposer_out_col_bits = *transposer_out_col_bits; // Mesher funnels transposer output to MeshHull
  hull_in.a_is_from_transposer = bit(a_from_transposer);
  hull_in.b_is_from_transposer = bit(b_from_transposer);
  hull_in.d_is_from_transposer = bit(d_from_transposer);
  hull_in.pe_control = cur_req_state.pe_control;
  hull_in.matmul_id  = cur_matmul_id;
  hull_in.last_fire  = bit(last_fire);
  hull_in.not_paused = bit(!pause);
  hull_.step(hull_in);

  // ************************************
  // Tracking Queues & Response Formation
  // ************************************
  const auto matmul_id_of_output  = wrappingAdd(cur_matmul_id, 2, static_cast<std::uint8_t>(kMaxSimultaneousMatmuls));
  const auto matmul_id_of_current = wrappingAdd(cur_matmul_id, 1, static_cast<std::uint8_t>(kMaxSimultaneousMatmuls));
  // tagq & total_rows_q logic (RTL relies on tagqlen sizing; the C++ model guards array writes explicitly)
  const bool metadata_queues_have_space = cur_tagq_count < kMesherTagQueueEntries && cur_total_rows_q_count < kMesherTagQueueEntries;
  // when non-flush req is accepted and queues have space (store metadata in tagq and total_rows_q)
  const bool enqueue_mesh_metadata = req_fire && req_bits->flush == 0 && metadata_queues_have_space;
  // queue local front-view/peek logic
  const bool tagq_front_valid = cur_tagq_count != 0;
  const auto tagq_front_bits  = tagq_front_valid ? tagq_[cur_tagq_head] : TagQEntry{};
  const bool total_rows_q_front_valid = cur_total_rows_q_count != 0;
  const auto total_rows_q_front_bits  = total_rows_q_front_valid ? total_rows_q_[cur_total_rows_q_head] : TotalRowsQEntry{};

  const auto hull_out   = hull_.out();
  const auto resp_data  = hull_out.resp_data;
  const bool resp_valid = hull_out.resp_valid != 0;
  const bool resp_last  = hull_out.resp_last != 0;
  const std::uint8_t out_matmul_id = hull_out.out_matmul_id;
  // is there a match between queue heads and MeshCore output mamtul ID
  const bool tagq_id_matches        = tagq_front_valid && out_matmul_id == tagq_front_bits.id;
  const bool total_rows_id_matches  = total_rows_q_front_valid && out_matmul_id == total_rows_q_front_bits.id;

  resp_val = bit(resp_valid); // response val comes straight from hull
  MesherResp next_resp_bits{};
  next_resp_bits.data       = resp_data; // response data comes straight from hull
  next_resp_bits.last       = bit(resp_last); // response last comes straight from hull
  next_resp_bits.tag        = tagq_id_matches ? tagq_front_bits.tag : makeGarbageTag();
  next_resp_bits.total_rows = total_rows_id_matches ? total_rows_q_front_bits.total_rows : static_cast<std::uint32_t>(kDim);
  resp_bits = next_resp_bits;

  // pop tagq when matching o/p ID appears and this is last o/p row for that tagged operation
  const bool tagq_deq_fire = resp_valid && resp_last && tagq_id_matches;
  // pop total_rows_q when matching o/p ID appears and this is last o/p for for that request
  const bool total_rows_q_deq_fire = resp_valid && resp_last && total_rows_id_matches;
  
  auto next_req_state          = cur_req_state;
  bool next_req_state_valid    = cur_req_state_valid;
  auto next_matmul_id          = cur_matmul_id;
  auto next_next_matmul_id     = cur_next_matmul_id;
  bool next_in_prop            = cur_in_prop;
  bool next_a_written          = cur_a_written;
  bool next_b_written          = cur_b_written;
  bool next_d_written          = cur_d_written;
  auto next_fire_counter       = cur_fire_counter;
  auto next_tagq               = tagq_;
  auto next_tagq_head          = cur_tagq_head;
  auto next_tagq_tail          = cur_tagq_tail;
  auto next_tagq_count         = cur_tagq_count;
  auto next_total_rows_q       = total_rows_q_;
  auto next_total_rows_q_head  = cur_total_rows_q_head;
  auto next_total_rows_q_tail  = cur_total_rows_q_tail;
  auto next_total_rows_q_count = cur_total_rows_q_count;

  // ************
  // State Update
  // ************
  if (req_fire) {
    next_req_state       = *req_bits; // push in new req
    next_req_state_valid = true;      // mark as valid
    next_in_prop         = (req_bits->pe_control.propagate != 0) != cur_in_prop;
    next_matmul_id       = wrappingAdd(cur_matmul_id, 1, static_cast<std::uint8_t>(kMaxSimultaneousMatmuls));
  } else if (last_fire) {
    next_req_state_valid = cur_req_state.flush > 1;
    next_req_state.flush = static_cast<u8>(cur_req_state.flush - 1);
  }

  if (a_fire) {
    next_a_written = true;
  }
  if (b_fire) {
    next_b_written = true;
  }
  if (d_fire) {
    next_d_written = true;
  }

  if (input_next_row_into_spatial_array) {
    next_fire_counter = wrappingAdd(cur_fire_counter, 1u, total_fires); // track how many rows of current req have entered mesh
    next_a_written    = false;
    next_b_written    = false;
    next_d_written    = false;
  }
  // enq tagq and total_rows_q
  if (enqueue_mesh_metadata) {
    next_tagq[cur_tagq_tail].tag = req_bits->tag;
    next_tagq[cur_tagq_tail].id  = matmul_id_of_output;
    next_tagq_tail               = wrappingAdd(cur_tagq_tail, 1, static_cast<std::uint8_t>(kMesherTagQueueEntries));
    next_tagq_count              = static_cast<std::uint8_t>(cur_tagq_count + 1);

    next_total_rows_q[cur_total_rows_q_tail].total_rows = req_bits->total_rows;
    next_total_rows_q[cur_total_rows_q_tail].id         = matmul_id_of_current;
    next_total_rows_q_tail                              = wrappingAdd(cur_total_rows_q_tail, 1, static_cast<std::uint8_t>(kMesherTagQueueEntries));
    next_total_rows_q_count                             = static_cast<std::uint8_t>(cur_total_rows_q_count + 1);
  }
  // deq tagq
  if (tagq_deq_fire) {
    // The raw tag array is exposed as tags_in_progress, so a popped slot must
    // stop looking like an in-flight memory destination immediately.
    next_tagq[next_tagq_head].tag = makeGarbageTag();
    next_tagq_head  = wrappingAdd(next_tagq_head, 1, static_cast<std::uint8_t>(kMesherTagQueueEntries));
    next_tagq_count = static_cast<std::uint8_t>(next_tagq_count - 1);
  }
  // deq total_rows_q
  if (total_rows_q_deq_fire) {
    next_total_rows_q_head  = wrappingAdd(next_total_rows_q_head, 1, static_cast<std::uint8_t>(kMesherTagQueueEntries));
    next_total_rows_q_count = static_cast<std::uint8_t>(next_total_rows_q_count - 1);
  }

  req_state_          = next_req_state;
  req_state_valid_    = next_req_state_valid;
  matmul_id_          = next_matmul_id;
  next_matmul_id_     = next_next_matmul_id;
  in_prop_            = next_in_prop;
  a_written_          = next_a_written;
  b_written_          = next_b_written;
  d_written_          = next_d_written;
  fire_counter_       = next_fire_counter;
  tagq_               = next_tagq;
  tagq_head_          = next_tagq_head;
  tagq_tail_          = next_tagq_tail;
  tagq_count_         = next_tagq_count;
  total_rows_q_       = next_total_rows_q;
  total_rows_q_head_  = next_total_rows_q_head;
  total_rows_q_tail_  = next_total_rows_q_tail;
  total_rows_q_count_ = next_total_rows_q_count;
}

void Mesher::reset() {
  req_state_          = ExCtrlMeshReq{};
  req_state_valid_    = false;
  matmul_id_          = 0;
  next_matmul_id_     = 0;
  in_prop_            = false;
  a_written_          = false;
  b_written_          = false;
  d_written_          = false;
  fire_counter_       = 0;
  tagq_               = {};
  // The systolic design represents every unused physical tag slot with a garbage address.
  for (auto& entry : tagq_) {
    entry.tag = makeGarbageTag();
  }
  tagq_head_          = 0;
  tagq_tail_          = 0;
  tagq_count_         = 0;
  total_rows_q_       = {};
  total_rows_q_head_  = 0;
  total_rows_q_tail_  = 0;
  total_rows_q_count_ = 0;
  hull_.reset();

  req_rdy.reset(0);
  transposer_in_row_val.reset(0);
  transposer_in_row_bits.reset(MeshInputRow{});
  resp_val.reset(0);
  resp_bits.reset(MesherResp{});
  for (std::size_t i = 0; i < kMesherTagQueueEntries; ++i) {
    tags_in_progress[i].reset(MesherTag{});
  }
}

} // namespace smesh
