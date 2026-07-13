// **********************************************************************
// smesh/src/StNormCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
Store-path normalization-stage control implementation.  Controls the flow of write data (from accum read resp port) and write metadata (from DmaWriteNormQueue).
Case 1: scratchpad or garbage
  norm_deq_rdy        = scale_enq_rdy
  scale_enq_val       = norm_deq_val
  normalizer_cmd_val  = 0
  accum_read_resp_rdy = 0

Case 2/3: accumulator for this bank
  norm_deq_rdy        = accum_read_resp_val && normalizer_cmd_rdy &&
  scale_enq_rdy
  normalizer_cmd_val  = norm_deq_val && accum_read_resp_val &&
  scale_enq_rdy
  accum_read_resp_rdy = norm_deq_val && normalizer_cmd_rdy &&
  scale_enq_rdy
  scale_enq_val       = full_accum_move && writes_to_main_memory

Accumulator data always goes toward the normalizer, but metadata only advances to write_scale_q when norm_cmd writes to main memory.  It writes to main memory when 3b norm_cmd sub-field in laddr field is set to 0 (RESET).  Note that this subfield can be set to other values (SUM, MEAN, VARIANCE, INV_STDDEV, MAX, SUM_EXP, INV_SUM_EXP) to indicate that the normalizer should consume the data to update stats, but not send any store-to-DRAM metadata onward.  Why do you collect stats? For normalization and activation operations on accumluator data.  For example, layer normalization (need mean, need variance / inverse stddev) and softmax (need max, need sum of exp, need inverse sum of exp) require statistics to be collected from the entire accumulator row before the normalization operation can be performed.  The normalizer consumes the data to update stats in its internal registers, but does not send any store-to-DRAM metadata onward until the stats have been collected and the norm_cmd is set to RESET.

The name is confusing because RESET here effectively means: this is not one of the stats-collection phases; after this, reset/finish the norm state and let data continue.
*/

#include "StNormCtrl.hpp"

namespace smesh {

namespace {

constexpr std::uint32_t kNormCmdReset = 0;
// check whether the norm_cmd subfield in laddr implies a store to main memory (i.e., norm_cmd == RESET)
bool normCmdWritesToMainMemory(std::uint32_t norm_cmd) {
  return norm_cmd == kNormCmdReset;
}

} // namespace

StNormCtrl::StNormCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(norm_deq_val,
                       norm_deq_bits,
                       accum_read_resp_val,
                       accum_read_resp_bits,
                       normalizer_cmd_rdy,
                       scale_enq_rdy,
                       bank_index)
                .writes(norm_deq_rdy,
                        scale_enq_val,
                        normalizer_cmd_val,
                        normalizer_req_bits,
                        accum_read_resp_rdy);
}

void StNormCtrl::update() {
  const auto req = *norm_deq_bits;
  const auto acc_resp = *accum_read_resp_bits;
  const auto laddr = req.laddr;

  const bool is_scratchpad = !laddr.is_acc_addr(); // metadata says we're writing from spad
  const bool is_garbage    = laddr.is_garbage();   // metadata says this is not a real write
  const bool bypass_normalizer = is_garbage || is_scratchpad; // not from accum, so bypass normalizer stage
  const bool targets_this_accum_bank =
      laddr.is_acc_addr() &&
      !is_garbage &&
      laddr.acc_bank() == static_cast<std::uint32_t>(bank_index); // metadata says this is write from accum
  const bool writes_to_main_memory = normCmdWritesToMainMemory(laddr.norm_cmd()); // is subfield norm_cmd==RESET?

  // just using next_* as a convenience (clear C++/Cascade separation)
  bool next_norm_deq_rdy        = false;
  bool next_scale_enq_val       = false;
  bool next_normalizer_cmd_val  = false; // default skip normalizer
  bool next_accum_read_resp_rdy = false; // default don't consume accum read resp
  AccNormReq next_normalizer_req{};
  next_normalizer_req.acc_read_resp = acc_resp;
  next_normalizer_req.cmd.len       = req.len;
  next_normalizer_req.cmd.stats_id  = req.acc_norm_stats_id;
  next_normalizer_req.cmd.cmd = static_cast<u8>(laddr.norm_cmd());

  // CASE 1: spad or garbage, bypass normalizer stage
  if (bypass_normalizer) {
    next_norm_deq_rdy = scale_enq_rdy != 0; // norm may pop if scale queue is ready
    next_scale_enq_val = norm_deq_val != 0; // scale input is valid if norm says so
  }
  // CASE 2/3: accumulator for this bank 
  else if (targets_this_accum_bank) {
    // all relevant parts have valid data and room to consume
    const bool accum_move = norm_deq_val != 0 &&
                            accum_read_resp_val != 0 &&
                            normalizer_cmd_rdy != 0 &&
                            scale_enq_rdy != 0;
    // let valid norm metadata pop if there's valid accum data, normalizer is ready, and scale queue is ready
    next_norm_deq_rdy = accum_read_resp_val != 0 &&
                        normalizer_cmd_rdy != 0 &&
                        scale_enq_rdy != 0;
    // let normalizer consume if there's valid norm medatdata, valid accum data, and scale queue is ready
    // (note: we don't necessarily have to wait for scale queue if norm_cmd != RESET, but we're too dumb to add this nuance)
    next_normalizer_cmd_val = norm_deq_val != 0 &&
                              accum_read_resp_val != 0 &&
                              scale_enq_rdy != 0;
    // let valid accum data pop if there's valid norm metadata, normalizer is ready, and scale queue is ready
    next_accum_read_resp_rdy = norm_deq_val != 0 &&
                               normalizer_cmd_rdy != 0 &&
                               scale_enq_rdy != 0;
    // let scalue queue consume if all relevant parts have valid data and room to consume and norm_cmd subfield is RESET
    next_scale_enq_val = accum_move && writes_to_main_memory;
  }
  // turn C++ booleans into Cascade bit signals for output
  norm_deq_rdy = bit(next_norm_deq_rdy);
  scale_enq_val = bit(next_scale_enq_val);
  normalizer_cmd_val = bit(next_normalizer_cmd_val);
  normalizer_req_bits = next_normalizer_req;
  accum_read_resp_rdy = bit(next_accum_read_resp_rdy);
}

} // namespace smesh
