// **********************************************************************
// smesh/src/tb_ex_ctrl_decoder.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 26 2026
// Focused ExCtrlDecoder command-window decode test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlDecoder.hpp"
#include "SmeshCommand.hpp"

#include <cstdio>

class DecoderDriver : public Component {
  DECLARE_COMPONENT(DecoderDriver);

 public:
  DecoderDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  OutputArray(bit, head_val, smesh::kExCtrlCmdWindow);
  OutputArray(smesh::SmeshIssue, head_bits, smesh::kExCtrlCmdWindow);
  Output(u8, current_dataflow);
  Output(bit, a_transpose);
  Output(bit, bd_transpose);
  Output(bit, ex_read_from_acc);
  Output(bit, ex_write_to_spad);
  OutputArray(smesh::MesherTag, tags_in_progress, smesh::kRsExecuteEntries);

  void update();
  void reset();

 private:
  int cycle_ = 0;
};

class DecoderMonitor : public Component {
  DECLARE_COMPONENT(DecoderMonitor);

 public:
  DecoderMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, do_config);
  InputArray(bit, do_computes, smesh::kExCtrlCmdWindow);
  InputArray(bit, do_preloads, smesh::kExCtrlCmdWindow);
  Input(bit, in_prop);
  Input(u8, preload_cmd_place);
  Input(smesh::SmeshLocalAddr, a_address_rs1);
  Input(smesh::SmeshLocalAddr, b_address_rs2);
  Input(smesh::SmeshLocalAddr, d_address_rs1);
  Input(smesh::SmeshLocalAddr, c_address_rs2);
  Input(u16, a_rows);
  Input(u16, a_cols);
  Input(u16, b_rows);
  Input(u16, b_cols);
  Input(u16, d_rows);
  Input(u16, d_cols);
  Input(u16, c_rows);
  Input(u16, c_cols);
  Input(bit, third_instruction_needed);
  Input(bit, matmul_in_progress);
  Input(bit, raw_hazard_pre);
  Input(bit, raw_hazard_mulpre);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  int cycle_ = 0;
  bool done_ = false;
  bool passed_ = false;
};

namespace {

smesh::SmeshIssue makeIssue(smesh::SmeshFunct funct, std::uint64_t rs1, std::uint64_t rs2) {
  smesh::SmeshIssue issue{};
  issue.cmd.funct = static_cast<std::uint32_t>(funct);
  issue.cmd.rs1 = u64(rs1);
  issue.cmd.rs2 = u64(rs2);
  return issue;
}

} // namespace

DecoderDriver::DecoderDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(head_val,
                        head_bits,
                        current_dataflow,
                        a_transpose,
                        bd_transpose,
                        ex_read_from_acc,
                        ex_write_to_spad,
                        tags_in_progress);
}

void DecoderDriver::update() {
  constexpr smesh::MatrixShape shape{smesh::kDim, smesh::kDim};
  const auto a_addr = smesh::makeSpAddr(0);
  const auto b_addr = smesh::makeSpAddr(4);
  const auto c_addr = smesh::makeAccAddr(8);

  head_val[0] = 1;
  head_bits[0] = makeIssue(smesh::SmeshFunct::Preload,
                           smesh::packLocal(b_addr, shape),
                           smesh::packLocal(c_addr, shape));
  head_val[1] = 1;
  head_bits[1] = makeIssue(smesh::SmeshFunct::ComputeFlip,
                           smesh::packLocal(a_addr, shape),
                           smesh::packLocal(b_addr, shape));
  head_val[2] = 0;
  head_bits[2] = smesh::SmeshIssue{};

  current_dataflow = smesh::kExDataflowWS;
  a_transpose = 0;
  bd_transpose = 0;
  ex_read_from_acc = 0;
  ex_write_to_spad = 0;
  for (std::size_t i = 0; i < smesh::kRsExecuteEntries; ++i) {
    tags_in_progress[i] = smesh::MesherTag{};
  }
  if (cycle_ == 1) {
    smesh::MesherTag active{};
    active.rs_tag_valid = 1;
    active.rs_tag = 99;
    tags_in_progress[0] = active;
  } else if (cycle_ == 2) {
    ex_read_from_acc = 1;
    smesh::MesherTag active{};
    active.rs_tag_valid = 1;
    active.rs_tag = 100;
    active.addr = a_addr;
    tags_in_progress[0] = active;
  }
  ++cycle_;
}

void DecoderDriver::reset() {
  cycle_ = 0;
  for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
    head_val[i].reset(0);
    head_bits[i].reset(smesh::SmeshIssue{});
  }
  current_dataflow.reset(smesh::kExDataflowWS);
  a_transpose.reset(0);
  bd_transpose.reset(0);
  ex_read_from_acc.reset(0);
  ex_write_to_spad.reset(0);
  for (std::size_t i = 0; i < smesh::kRsExecuteEntries; ++i) {
    tags_in_progress[i].reset(smesh::MesherTag{});
  }
}

DecoderMonitor::DecoderMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(do_config,
             do_computes,
             do_preloads,
             in_prop,
             preload_cmd_place,
             a_address_rs1,
             b_address_rs2)
      .reads(d_address_rs1,
             c_address_rs2,
             a_rows,
             a_cols,
             b_rows,
             b_cols,
             d_rows,
             d_cols)
      .reads(c_rows,
             c_cols,
             third_instruction_needed,
             matmul_in_progress,
             raw_hazard_pre,
             raw_hazard_mulpre);
}

void DecoderMonitor::update() {
  if (Sim::state == Sim::SimResetting || done_) {
    return;
  }

  const bool classes_ok =
      do_config == 0 &&
      do_preloads[0] != 0 &&
      do_computes[1] != 0 &&
      do_preloads[1] == 0 &&
      do_computes[0] == 0 &&
      in_prop == 0;
  const bool places_ok =
      preload_cmd_place == 0;
  const bool addrs_ok =
      (*a_address_rs1).raw == smesh::makeSpAddr(0).raw &&
      (*b_address_rs2).raw == smesh::makeSpAddr(4).raw &&
      (*d_address_rs1).raw == smesh::makeSpAddr(4).raw &&
      (*c_address_rs2).raw == smesh::makeAccAddr(8).raw;
  const bool dims_ok =
      a_rows == smesh::kDim && a_cols == smesh::kDim &&
      b_rows == smesh::kDim && b_cols == smesh::kDim &&
      d_rows == smesh::kDim && d_cols == smesh::kDim &&
      c_rows == smesh::kDim && c_cols == smesh::kDim;
  const bool hazards_ok = third_instruction_needed == 0 &&
                          matmul_in_progress == 0;

  if (cycle_ == 0) {
    passed_ = classes_ok && places_ok && addrs_ok && dims_ok && hazards_ok;
  } else if (cycle_ == 1) {
    passed_ = passed_ &&
              matmul_in_progress != 0 &&
              raw_hazard_pre == 0 &&
              raw_hazard_mulpre == 0;
  } else if (cycle_ == 2) {
    passed_ = passed_ &&
              matmul_in_progress != 0 &&
              raw_hazard_pre != 0 &&
              raw_hazard_mulpre != 0;
    done_ = true;
  }

  ++cycle_;
}

void DecoderMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = false;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  DecoderDriver driver("Driver");
  smesh::ExCtrlDecoder decoder("Decoder");
  DecoderMonitor monitor("Monitor");

  for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
    decoder.head_val[i] << driver.head_val[i];
    decoder.head_bits[i] << driver.head_bits[i];
    monitor.do_computes[i] << decoder.do_computes[i];
    monitor.do_preloads[i] << decoder.do_preloads[i];
  }
  decoder.current_dataflow << driver.current_dataflow;
  decoder.a_transpose << driver.a_transpose;
  decoder.bd_transpose << driver.bd_transpose;
  decoder.ex_read_from_acc << driver.ex_read_from_acc;
  decoder.ex_write_to_spad << driver.ex_write_to_spad;
  for (std::size_t i = 0; i < smesh::kRsExecuteEntries; ++i) {
    decoder.tags_in_progress[i] << driver.tags_in_progress[i];
  }

  monitor.do_config << decoder.do_config;
  monitor.in_prop << decoder.in_prop;
  monitor.preload_cmd_place << decoder.preload_cmd_place;
  monitor.a_address_rs1 << decoder.a_address_rs1;
  monitor.b_address_rs2 << decoder.b_address_rs2;
  monitor.d_address_rs1 << decoder.d_address_rs1;
  monitor.c_address_rs2 << decoder.c_address_rs2;
  monitor.a_rows << decoder.a_rows;
  monitor.a_cols << decoder.a_cols;
  monitor.b_rows << decoder.b_rows;
  monitor.b_cols << decoder.b_cols;
  monitor.d_rows << decoder.d_rows;
  monitor.d_cols << decoder.d_cols;
  monitor.c_rows << decoder.c_rows;
  monitor.c_cols << decoder.c_cols;
  monitor.third_instruction_needed << decoder.third_instruction_needed;
  monitor.matmul_in_progress << decoder.matmul_in_progress;
  monitor.raw_hazard_pre << decoder.raw_hazard_pre;
  monitor.raw_hazard_mulpre << decoder.raw_hazard_mulpre;

  Clock clk;
  driver.clk << clk;
  decoder.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_DECODER] %s preload_compute_window\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
