// **********************************************************************
// smesh/include/mvin/MvinScale.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Load-path scale stages. They latch load-responses, process them and forward them to
downstream blocks.

Current functions:
- Repeated row outputs (multiple local rows from one DMA response).
- Binary32 scaling of normal-width signed 8-bit elements; full-width accumulator rows pass through.
The four-cycle scale-unit pipeline from Original is not modeled yet.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

struct MvinRepeatEntry {
  bit valid     = 0;
  DmaReadResp bits{};
  u16 remaining = 0;  // number of repeats remaining after the current one
};

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
  void updateStorage();
  void reset();

 private:
  Output(MvinRepeatEntry, entry_Q_);
  Register(MvinRepeatEntry, entry_D_);
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
  Output(MvinRepeatEntry, entry_Q_);
  Register(MvinRepeatEntry, entry_D_);
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
