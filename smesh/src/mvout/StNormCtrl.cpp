// **********************************************************************
// smesh/src/mvout/StNormCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
Arbitrates accumulator responses into the shared normalizer. Store responses
advance matching Store metadata; ExCtrl responses do not touch Store queues.
Scratchpad and garbage Store metadata bypass the normalizer.
*/

#include "StNormCtrl.hpp"

TraceKey(st_norm_view);

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
                       scale_enq_rdy)
                .writes(norm_deq_rdy,
                        scale_enq_val,
                        normalizer_cmd_val,
                        normalizer_req_bits,
                        accum_read_resp_rdy);
}

void StNormCtrl::update() {
  // *** Identify the Store metadata from write norm queue head ***
  const auto req         = *norm_deq_bits;
  const auto laddr       = req.laddr;
  const bool store_accum = norm_deq_val != 0 && laddr.is_acc_addr() && !laddr.is_garbage(); // there is valid Store metadata and it describes a real accum addr
  const auto store_bank  = laddr.acc_bank(); // accum bank for whose resp legit store metadata is waiting

  // *** Choose which Accum response to handle ***
  // matching Store response has priority. Otherwise, take first ExCtrl resp.
  std::size_t selected_bank = kAccBanks; // this preset indicates that no bank was selected
  // look for resp that matches Store metadata, if any
  if (store_accum && store_bank < kAccBanks && accum_read_resp_val[store_bank] != 0 && accum_read_resp_bits[store_bank]->from_dma != 0 && scale_enq_rdy != 0) {
    selected_bank = store_bank; // select Store's bank
  } else { // otherwise look for first ExCtrl resp (from_dma == 0) and select first such bank
    for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
      if (accum_read_resp_val[bank] != 0 && accum_read_resp_bits[bank]->from_dma == 0) {
        selected_bank = bank;
        break;
      }
    }
  }

  // *** Build Normalizer input ***
  const bool selected_store = selected_bank < kAccBanks && accum_read_resp_bits[selected_bank]->from_dma != 0; // identifies if resp belongs to Store/DMA path
  const bool selected_valid = selected_bank < kAccBanks; // checks whether a bank was selected
  const bool selected_fire  = selected_valid && normalizer_cmd_rdy != 0; 

  // prepare the payload
  AccNormReq normalizer_req{};
  if (selected_valid) {
    normalizer_req.acc_read_resp = *accum_read_resp_bits[selected_bank];
    normalizer_req.cmd.len       = selected_store ? req.len               : normalizer_req.acc_read_resp.len;
    normalizer_req.cmd.stats_id  = selected_store ? req.acc_norm_stats_id : u16(0);
    normalizer_req.cmd.cmd       = selected_store ? u8(laddr.norm_cmd())  : u8(kNormCmdReset);
  }
  // drive the handshakes that transfer the payload to the normalizer, two cases:
  // (1) if Store metadata is garbage or describes a Spad addr, then bypass the normalizer 
  // (2) if Store metadata describes an Accum read, then Store metadata and matching Accum resp move together
  const bool bypass_store = norm_deq_val != 0 && (laddr.is_garbage() || !laddr.is_acc_addr()); // if Spad or garbage data corresponds to this metadata, bypass the Normalizer
  norm_deq_rdy        = bit(bypass_store ? scale_enq_rdy != 0 : selected_store && selected_fire);
  scale_enq_val       = bit(bypass_store || (selected_store && selected_fire && normCmdWritesToMainMemory(laddr.norm_cmd())));

  normalizer_cmd_val  = bit(selected_valid);
  normalizer_req_bits = normalizer_req;

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_read_resp_rdy[bank] = bit(selected_fire && bank == selected_bank);
  }
  if (selected_fire || (bypass_store && scale_enq_rdy == 1)) {
    trace(st_norm_view, "norm=%u bank=%u selected=%u dma=%u nrdy=%u srdy=%u nval=%u nready=%u scale=%u\n",
          static_cast<unsigned>(norm_deq_val), static_cast<unsigned>(store_bank),
          static_cast<unsigned>(selected_bank), static_cast<unsigned>(selected_store),
          static_cast<unsigned>(norm_deq_rdy), static_cast<unsigned>(scale_enq_rdy),
          static_cast<unsigned>(normalizer_cmd_val), static_cast<unsigned>(normalizer_cmd_rdy),
          static_cast<unsigned>(scale_enq_val));
  }
}

} // namespace smesh
