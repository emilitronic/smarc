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
- Configurable normal-width row latency with elastic backpressure.
Latency counts cycles from input acceptance to output visibility. Identity-scale
rows skip arithmetic but still pass through the configured row stages. The model
does not schedule original's individual scale units.  That is, original defaults 
using 4 parallel scaling unit pipelines (each pipeline requiring 4 cycles to complete).
Thus rows that consist of 4 elements would have to be split into chunks of 4 elements
and sent down the pipeline (followed by the next 4 elements on the next cycle and so on).
Our system does not currently model such consecutive row-chunk processing, but rather
handles an entire row in the alloted latency cycles (but does accept consecutive
full rows into the pipeline each cycle if needed).)
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

struct MvinPipeEntry {
  bit valid = 0;
  DmaReadResp bits{};
};

class MvinScale : public Component {
  DECLARE_COMPONENT(MvinScale);

 public:
  // Accepted rows become visible after latency cycles (default: one).
  MvinScale(std::string name, int latency = 1, COMPONENT_CTOR);

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
  void updateStages();
  void reset();

 private:
  bool rowAdvanceReady() const;

  Output(MvinRepeatEntry,      entry_Q_);
  Register(MvinRepeatEntry,    entry_D_);
  OutputArray(MvinPipeEntry,   stages_Q_);
  RegisterArray(MvinPipeEntry, stages_D_);
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
