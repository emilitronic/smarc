// **********************************************************************
// smesh/include/WriteCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 23 2026
/*
Load-return local-memory write control.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"
#include "SmeshTypes.hpp"

namespace smesh {

class WriteCtrl : public Component {
  DECLARE_COMPONENT(WriteCtrl);

 public:
  WriteCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, dmaread_spad_val);
  Input(DmaReadResp, dmaread_spad_bits);
  Output(bit, dmaread_spad_rdy);

  Input(bit, dmaread_accum_val);
  Input(DmaReadResp, dmaread_accum_bits);
  Output(bit, dmaread_accum_rdy);

  Input(bit, dmaread_accum_full_val);
  Input(DmaReadResp, dmaread_accum_full_bits);
  Output(bit, dmaread_accum_full_rdy);

  InputArray(bit, arb_spad_dmaread_rdy, kSpBanks);
  OutputArray(bit, arb_spad_dmaread_val, kSpBanks);
  OutputArray(DmaReadResp, arb_spad_dmaread_bits, kSpBanks);

  InputArray(bit, arb_accum_dmaread_rdy, kAccBanks);
  OutputArray(bit, arb_accum_dmaread_val, kAccBanks);
  OutputArray(DmaReadResp, arb_accum_dmaread_bits, kAccBanks);

  InputArray(bit, arb_accum_dmaread_full_rdy, kAccBanks);
  OutputArray(bit, arb_accum_dmaread_full_val, kAccBanks);
  OutputArray(DmaReadResp, arb_accum_dmaread_full_bits, kAccBanks);

  void update();
  void reset();
};

} // namespace smesh
