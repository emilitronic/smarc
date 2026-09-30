// **********************************************************************
// smesh/include/memory/ArbWriteLocal.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 23 2026
/*
Local-memory write arbiters.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class ArbWriteSpad : public Component {
  DECLARE_COMPONENT(ArbWriteSpad);

 public:
  ArbWriteSpad(std::string name, std::size_t bank, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,              exwrite_val);
  Input(SpadBankWriteReq, exwrite_bits);
  Output(bit,             exwrite_rdy);

  Input(bit,              dmaread_val);
  Input(DmaReadResp,      dmaread_bits);
  Output(bit,             dmaread_rdy);

  Input(bit,              zerowrite_val);
  Input(DmaReadResp,      zerowrite_bits);
  Output(bit,             zerowrite_rdy);

  Output(bit,             write_val);
  Input(bit,              write_rdy);
  Output(DmaReadResp,     write_bits);

  void updateReady();
  void updateWrite();
  void reset();

 private:
  std::size_t bank_; // one arbiter created per spad bank, so this is the bank index
};

class ArbWriteAccum : public Component {
  DECLARE_COMPONENT(ArbWriteAccum);

 public:
  ArbWriteAccum(std::string name, std::size_t bank, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,               exwrite_val);
  Input(AccumBankWriteReq, exwrite_bits);
  Output(bit,              exwrite_rdy);

  Input(bit,               dmaread_val);
  Input(DmaReadResp,       dmaread_bits);
  Output(bit,              dmaread_rdy);

  Input(bit,               dmaread_full_val);
  Input(DmaReadResp,       dmaread_full_bits);
  Output(bit,              dmaread_full_rdy);

  Input(bit,               zerowrite_val);
  Input(DmaReadResp,       zerowrite_bits);
  Output(bit,              zerowrite_rdy);

  Output(bit,              write_val);
  Input(bit,               write_rdy);
  Output(DmaReadResp,      write_bits);

  void updateReady();
  void updateWrite();
  void reset();

 private:
  std::size_t bank_; // one arbiter created per accum bank, so this is the bank index
};

} // namespace smesh
