// **********************************************************************
// smesh/src/ArbReadLocal.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 15 2026
/*
Local-memory read arbiter implementations.  Helps connect Ld/St/ExCtrl to local memory ports.
*/

#include "ArbReadLocal.hpp"

namespace smesh {

ArbReadSpad::ArbReadSpad(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateRequest)
      .reads(exread_val, exread_bits, dmawrite_val, dmawrite_bits)
      .writes(read_req_val, read_req_bits);
  UPDATE(updateClientReady)
      .reads(exread_val, dmawrite_val, read_req_rdy)
      .writes(exread_rdy, dmawrite_rdy);
}

void ArbReadSpad::updateRequest() {
  const bool exread   = exread_val   != 0; // ExCtrl is asking to read spad this cycle
  const bool dmawrite = dmawrite_val != 0; // store path asking to read spad this cycle

  read_req_val  = bit(exread || dmawrite); // if either Ex or St path wants to read spad, send valid
  read_req_bits = exread ? *exread_bits : *dmawrite_bits; // choose request payload to put in spad
}

void ArbReadSpad::updateClientReady() {
  const bool exread   = exread_val != 0;
  const bool dmawrite = dmawrite_val != 0;
  exread_rdy   = bit(exread && read_req_rdy != 0);
  dmawrite_rdy = bit(!exread && dmawrite && read_req_rdy != 0);
}

ArbReadAccum::ArbReadAccum(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateRequest)
      .reads(exread_val, exread_bits, dmawrite_val, dmawrite_bits)
      .writes(read_req_val, read_req_bits);
  UPDATE(updateClientReady)
      .reads(exread_val, dmawrite_val, read_req_rdy)
      .writes(exread_rdy, dmawrite_rdy);
}

void ArbReadAccum::updateRequest() {
  const bool exread   = exread_val == 1;
  const bool dmawrite = dmawrite_val == 1;

  read_req_val  = bit(exread || dmawrite);
  read_req_bits = exread ? *exread_bits : *dmawrite_bits;
}

void ArbReadAccum::updateClientReady() {
  const bool exread   = exread_val == 1;
  const bool dmawrite = dmawrite_val == 1;
  exread_rdy   = bit(exread && read_req_rdy == 1);
  dmawrite_rdy = bit(!exread && dmawrite && read_req_rdy == 1);
}

ArbRespSpad::ArbRespSpad(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(read_resp_val, read_resp_bits, dma_resp_rdy, ex_resp_rdy)
      .writes(read_resp_rdy);
}

void ArbRespSpad::update() {
  const auto resp = *read_resp_bits;
  const bool selected_ready = resp.from_dma != 0 ? dma_resp_rdy != 0 : ex_resp_rdy != 0;
  read_resp_rdy = bit(read_resp_val != 0 && selected_ready);
}
// send respones back to ExCtrl (for ex to accum read reqs)
AccumExResp::AccumExResp(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateRespView)
      .reads(acc_val, acc_bits)
      .writes(ex_resp_val, ex_resp_bits);
  UPDATE(updateRespReady)
      .reads(acc_val, acc_bits, ex_resp_rdy)
      .writes(acc_rdy_exresp);
}

void AccumExResp::updateRespView() {
  const auto acc = *acc_bits;
  const bool is_ex_resp = acc_val != 0 && acc.from_dma == 0;
  const auto bank = static_cast<std::size_t>(acc.acc_bank_id);

  for (std::size_t i = 0; i < kAccBanks; ++i) {
    ex_resp_val[i] = bit(is_ex_resp && i == bank);

    ExCtrlAccumReadResp resp{};
    resp.data = acc.data;
    resp.acc_bank_id = acc.acc_bank_id;
    resp.from_dma = false;
    ex_resp_bits[i] = resp;
  }
}

void AccumExResp::updateRespReady() {
  const auto acc = *acc_bits;
  const bool is_ex_resp = acc_val != 0 && acc.from_dma == 0;
  const auto bank = static_cast<std::size_t>(acc.acc_bank_id);
  acc_rdy_exresp = bit(is_ex_resp && bank < kAccBanks && ex_resp_rdy[bank] != 0);
}

} // namespace smesh
