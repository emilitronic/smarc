// **********************************************************************
// smesh/include/MvinScale.hpp
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

  FifoInput(DmaReadResp, data_in);
  FifoOutput(DmaReadResp, data_out);

  void update();
};

class MvinScaleAcc : public Component {
  DECLARE_COMPONENT(MvinScaleAcc);

 public:
  MvinScaleAcc(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(DmaReadResp, data_in);
  FifoOutput(DmaReadResp, data_out);
  Output(bit, data_val);
  Output(DmaReadResp, data_bits);
  Input(bit, data_rdy);

  void updateView();
  void update();
};

class MvinScaleSplit : public Component {
  DECLARE_COMPONENT(MvinScaleSplit);

 public:
  MvinScaleSplit(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(DmaReadResp, data_in);
  FifoOutput(DmaReadResp, normal_out);
  FifoOutput(DmaReadResp, acc_out);

  void update();
};

} // namespace smesh
