// **********************************************************************
// smesh/tests/unit/st_ctrl/tb_st_ctrl_cmd_dec.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
// Focused store-controller command decoder test.
/*
  It checks:

  - normal STORE decoding;
  - STORE_SPAD destination and stride;
  - CONFIG_STORE fields;
  - CONFIG_NORM fields;
  - rows, columns, and block calculation.
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "SmeshCommand.hpp"
#include "StCtrlCmdDec.hpp"

#include <cstdio>

namespace {

constexpr std::uint64_t kVaddr = 0x123456789abcdef0ull;

smesh::SmeshIssue issue(smesh::SmeshFunct funct, std::uint64_t rs1, std::uint64_t rs2) {
  smesh::SmeshIssue value{};
  value.cmd.funct = static_cast<std::uint32_t>(funct);
  value.cmd.rs1 = u64(rs1);
  value.cmd.rs2 = u64(rs2);
  return value;
}

std::uint64_t packConfigStoreRs1() {
  return 2u | (1ull << 2) | (2ull << 4) | (3ull << 6) |
         (1ull << 8) | (2ull << 10) | (7ull << 24) |
         (8ull << 32) | (9ull << 40) | (10ull << 48) | (11ull << 56);
}

std::uint64_t packConfigStoreRs2() {
  return 0x89abcdefull | (0x76543210ull << 32);
}

std::uint64_t packConfigNormRs1() {
  return 3u | (0x5aull << 8) | (1ull << 16) | (1ull << 17) |
         (1ull << 18) | (0xcafebabeull << 32);
}

} // namespace

class DecDriver : public Component {
  DECLARE_COMPONENT(DecDriver);

 public:
  DecDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, head_val);
  Output(smesh::SmeshIssue, head_bits);

  void update();
  void reset();

 private:
  unsigned cycle_ = 0;
};

class DecMonitor : public Component {
  DECLARE_COMPONENT(DecMonitor);

 public:
  DecMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, do_config);
  Input(bit, do_config_norm);
  Input(bit, do_store);
  Input(bit, dst_is_spad);
  Input(u64, vaddr);
  Input(smesh::SmeshLocalAddr, dst_spad_addr);
  Input(u32, dst_spad_stride);
  Input(smesh::SmeshLocalAddr, localaddr);
  Input(u32, rows);
  Input(u32, cols);
  Input(u32, blocks);
  Input(u8, config_cmd_type);
  Input(u32, config_stride);
  Input(u8, config_activation);
  Input(u32, config_acc_scale);
  Input(u8, config_pool_stride);
  Input(u8, config_pool_size);
  Input(u8, config_pool_out_dim);
  Input(u8, config_porows);
  Input(u8, config_pocols);
  Input(u8, config_orows);
  Input(u8, config_ocols);
  Input(u8, config_upad);
  Input(u8, config_lpad);
  Input(u8, config_stats_id);
  Input(bit, config_activation_msb);
  Input(bit, config_set_stats_id_only);
  Input(bit, config_iexp_q_const_type);
  Input(u32, config_iexp_q_const);
  Input(u32, config_igelu_qb);
  Input(u32, config_igelu_qc);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  unsigned cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

DecDriver::DecDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(head_val, head_bits);
}

void DecDriver::update() {
  head_val = 1;
  switch (cycle_) {
    case 0:
      head_bits = issue(smesh::SmeshFunct::Mvout,
                        kVaddr,
                        smesh::packLocal(smesh::makeSpAddr(5), {3, 5}));
      break;
    case 1:
      head_bits = issue(smesh::SmeshFunct::StoreSpad,
                        smesh::packStoreSpadDestination(smesh::makeSpAddr(9), 7),
                        smesh::packLocal(smesh::makeAccAddr(4), {2, 6}));
      break;
    case 2:
      head_bits = issue(smesh::SmeshFunct::Config,
                        packConfigStoreRs1(),
                        packConfigStoreRs2());
      break;
    default:
      head_bits = issue(smesh::SmeshFunct::Config,
                        packConfigNormRs1(),
                        0x11223344ull | (0x55667788ull << 32));
      break;
  }
  ++cycle_;
}

void DecDriver::reset() {
  cycle_ = 0;
  head_val.reset(0);
  head_bits.reset(smesh::SmeshIssue{});
}

DecMonitor::DecMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(do_config,
             do_config_norm,
             do_store,
             dst_is_spad,
             vaddr,
             dst_spad_addr,
             dst_spad_stride,
             localaddr)
      .reads(rows,
             cols,
             blocks,
             config_cmd_type,
             config_stride,
             config_activation,
             config_acc_scale,
             config_pool_stride)
      .reads(config_pool_size,
             config_pool_out_dim,
             config_porows,
             config_pocols,
             config_orows,
             config_ocols,
             config_upad,
             config_lpad)
      .reads(config_stats_id,
             config_activation_msb,
             config_set_stats_id_only,
             config_iexp_q_const_type,
             config_iexp_q_const,
             config_igelu_qb,
             config_igelu_qc);
}

void DecMonitor::update() {
  if (Sim::state == Sim::SimResetting || done_) {
    return;
  }

  bool ok = false;
  if (cycle_ == 0) {
    ok = do_store != 0 && do_config == 0 && do_config_norm == 0 &&
         dst_is_spad == 0 && *vaddr == kVaddr &&
         localaddr->raw == smesh::makeSpAddr(5).raw &&
         rows == 3 && cols == 5 && blocks == 2;
  } else if (cycle_ == 1) {
    ok = do_store != 0 && dst_is_spad != 0 &&
         dst_spad_addr->raw == smesh::makeSpAddr(9).raw &&
         *dst_spad_stride == 7 && localaddr->raw == smesh::makeAccAddr(4).raw &&
         rows == 2 && cols == 6 && blocks == 2;
  } else if (cycle_ == 2) {
    ok = do_config != 0 && do_config_norm == 0 && do_store == 0 &&
         *config_cmd_type == 2 && *config_activation == 1 &&
         *config_pool_stride == 2 && *config_pool_size == 3 &&
         *config_upad == 1 && *config_lpad == 2 &&
         *config_pool_out_dim == 7 && *config_porows == 8 &&
         *config_pocols == 9 && *config_orows == 10 && *config_ocols == 11 &&
         *config_stride == 0x89abcdefu &&
         *config_acc_scale == 0x76543210u;
  } else {
    ok = do_config == 0 && do_config_norm != 0 && do_store == 0 &&
         *config_cmd_type == 3 && *config_stats_id == 0x5a &&
         *config_activation_msb != 0 && *config_set_stats_id_only != 0 &&
         *config_iexp_q_const_type != 0 &&
         *config_iexp_q_const == 0xcafebabeu &&
         *config_igelu_qb == 0x11223344u &&
         *config_igelu_qc == 0x55667788u;
  }

  passed_ = passed_ && ok;
  ++cycle_;
  done_ = cycle_ == 4;
}

void DecMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = true;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  DecDriver driver("Driver");
  smesh::StCtrlCmdDec decoder("StCtrlCmdDec");
  DecMonitor monitor("Monitor");

  decoder.head_val << driver.head_val;
  decoder.head_bits << driver.head_bits;

  monitor.do_config << decoder.do_config;
  monitor.do_config_norm << decoder.do_config_norm;
  monitor.do_store << decoder.do_store;
  monitor.dst_is_spad << decoder.dst_is_spad;
  monitor.vaddr << decoder.vaddr;
  monitor.dst_spad_addr << decoder.dst_spad_addr;
  monitor.dst_spad_stride << decoder.dst_spad_stride;
  monitor.localaddr << decoder.localaddr;
  monitor.rows << decoder.rows;
  monitor.cols << decoder.cols;
  monitor.blocks << decoder.blocks;
  monitor.config_cmd_type << decoder.config_cmd_type;
  monitor.config_stride << decoder.config_stride;
  monitor.config_activation << decoder.config_activation;
  monitor.config_acc_scale << decoder.config_acc_scale;
  monitor.config_pool_stride << decoder.config_pool_stride;
  monitor.config_pool_size << decoder.config_pool_size;
  monitor.config_pool_out_dim << decoder.config_pool_out_dim;
  monitor.config_porows << decoder.config_porows;
  monitor.config_pocols << decoder.config_pocols;
  monitor.config_orows << decoder.config_orows;
  monitor.config_ocols << decoder.config_ocols;
  monitor.config_upad << decoder.config_upad;
  monitor.config_lpad << decoder.config_lpad;
  monitor.config_stats_id << decoder.config_stats_id;
  monitor.config_activation_msb << decoder.config_activation_msb;
  monitor.config_set_stats_id_only << decoder.config_set_stats_id_only;
  monitor.config_iexp_q_const_type << decoder.config_iexp_q_const_type;
  monitor.config_iexp_q_const << decoder.config_iexp_q_const;
  monitor.config_igelu_qb << decoder.config_igelu_qb;
  monitor.config_igelu_qc << decoder.config_igelu_qc;

  Clock clk;
  driver.clk << clk;
  decoder.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 6 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[ST_CTRL_CMD_DEC] %s head_decode\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
