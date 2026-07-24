// **********************************************************************
// smesh/include/ArbWriteLocal.hpp
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
  ArbWriteSpad(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, exwrite_val);
  Input(DmaReadResp, exwrite_bits);
  Output(bit, exwrite_rdy);

  Input(bit, dmaread_val);
  Input(DmaReadResp, dmaread_bits);
  Output(bit, dmaread_rdy);

  Input(bit, zerowrite_val);
  Input(DmaReadResp, zerowrite_bits);
  Output(bit, zerowrite_rdy);

  Output(bit, write_val);
  Input(bit, write_rdy);
  Output(DmaReadResp, write_bits);

  void updateReady();
  void updateWrite();
  void reset();
};

class ArbWriteAccum : public Component {
  DECLARE_COMPONENT(ArbWriteAccum);

 public:
  ArbWriteAccum(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, exwrite_val);
  Input(DmaReadResp, exwrite_bits);
  Output(bit, exwrite_rdy);

  Input(bit, dmaread_val);
  Input(DmaReadResp, dmaread_bits);
  Output(bit, dmaread_rdy);

  Input(bit, dmaread_full_val);
  Input(DmaReadResp, dmaread_full_bits);
  Output(bit, dmaread_full_rdy);

  Input(bit, zerowrite_val);
  Input(DmaReadResp, zerowrite_bits);
  Output(bit, zerowrite_rdy);

  Output(bit, write_val);
  Input(bit, write_rdy);
  Output(DmaReadResp, write_bits);

  void updateReady();
  void updateWrite();
  void reset();
};

} // namespace smesh
