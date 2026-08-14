// **********************************************************************
// smesh/include/ArbReadLocal.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 15 2026
/*
Local-memory read arbiters.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class ArbReadSpad : public Component {
  DECLARE_COMPONENT(ArbReadSpad);

 public:
  ArbReadSpad(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, exread_val);
  Input(SpadReadReq, exread_bits);
  Output(bit, exread_rdy);

  Input(bit, dmawrite_val);
  Input(SpadReadReq, dmawrite_bits);
  Output(bit, dmawrite_rdy);

  Output(bit, read_req_val);
  Input(bit, read_req_rdy);
  Output(SpadReadReq, read_req_bits);

  void update();
};

class ArbReadAccum : public Component {
  DECLARE_COMPONENT(ArbReadAccum);

 public:
  ArbReadAccum(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, exread_val);
  Input(AccumReadReq, exread_bits);
  Output(bit, exread_rdy);

  Input(bit, dmawrite_val);
  Input(AccumReadReq, dmawrite_bits);
  Output(bit, dmawrite_rdy);

  Output(bit, read_req_val);
  Input(bit, read_req_rdy);
  Output(AccumReadReq, read_req_bits);

  void update();
};

class ArbRespSpad : public Component {
  DECLARE_COMPONENT(ArbRespSpad);

 public:
  ArbRespSpad(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, read_resp_val);
  Input(SpadReadResp, read_resp_bits);
  Input(bit, dma_resp_rdy);
  Input(bit, ex_resp_rdy);

  Output(bit, read_resp_rdy);

  void update();
};

class AccumExResp : public Component {
  DECLARE_COMPONENT(AccumExResp);

 public:
  AccumExResp(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, acc_val);
  Input(AccScaleResp, acc_bits);
  Output(bit, acc_rdy_exresp);

  OutputArray(bit, ex_resp_val, kAccBanks);
  OutputArray(ExCtrlAccumReadResp, ex_resp_bits, kAccBanks);
  InputArray(bit, ex_resp_rdy, kAccBanks);

  void update();
};

} // namespace smesh
