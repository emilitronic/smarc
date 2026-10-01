// **********************************************************************
// smesh/include/mvin/MvinScale.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Load-path scaling stage. Scaling is currently an identity operation.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class MvinScale : public Component {
  DECLARE_COMPONENT(MvinScale);

 public:
  MvinScale(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,          in_val);
  Input(DmaReadResp,  in_bits);
  Output(bit,         in_rdy);
  Output(bit,         out_val);
  Output(DmaReadResp, out_bits);
  Input(bit,          out_rdy);

  void updateView();
  void updateReady();
  void reset();
};

class MvinScaleAcc : public Component {
  DECLARE_COMPONENT(MvinScaleAcc);

 public:
  MvinScaleAcc(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,          in_val);
  Input(DmaReadResp,  in_bits);
  Output(bit,         in_rdy);
  Output(bit,         out_val);
  Output(DmaReadResp, out_bits);
  Input(bit,          out_rdy);

  void updateView();
  void updateReady();
  void updateStorage();
  void reset();

 private:
  struct Entry {
    bit valid = 0;
    DmaReadResp bits{};
  };

  Output(Entry, entry_Q_);
  Register(Entry, entry_D_);
};

class MvinScaleSplit : public Component {
  DECLARE_COMPONENT(MvinScaleSplit);

 public:
  MvinScaleSplit(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,          in_val);
  Input(DmaReadResp,  in_bits);
  Output(bit,         in_rdy);
  Output(bit,         normal_val);
  Output(DmaReadResp, normal_bits);
  Input(bit,          normal_rdy);
  Output(bit,         acc_val);
  Output(DmaReadResp, acc_bits);
  Input(bit,          acc_rdy);

  void updateView();
  void updateReady();
  void reset();
};

} // namespace smesh
