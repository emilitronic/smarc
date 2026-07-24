// **********************************************************************
// smesh/src/tb_smesh_top_acc_load.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 9 2026
// Focused SmeshTop load-path test from external memory into accumulator.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "SmeshCommand.hpp"
#include "SmeshTop.hpp"
#include "smem/Dram.hpp"
#include "smem/MemCtrl.hpp"

#include <array>
#include <cstdio>

constexpr std::uint64_t kDramBase = 0x80004000;
constexpr std::uint32_t kDramRowStride = 9;
constexpr std::uint32_t kLoadBlockStride = 5;

class TopAccLoadDriver : public Component {
  DECLARE_COMPONENT(TopAccLoadDriver);

 public:
  TopAccLoadDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, cmd_valid);
  Output(smesh::SmeshCmd, cmd_bits);
  Input(bit, cmd_ready);

  void update();
  void reset();

 private:
  std::uint32_t next_command_ = 0;
};

TopAccLoadDriver::TopAccLoadDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cmd_ready).writes(cmd_valid, cmd_bits);
}

void TopAccLoadDriver::update() {
  cmd_valid = 0;
  cmd_bits = smesh::SmeshCmd{};
  if (Sim::state == Sim::SimResetting) {
    return;
  }
  if (next_command_ >= 2) {
    return;
  }

  constexpr smesh::MatrixShape shape{smesh::kDim, smesh::kDim};
  smesh::SmeshCmd cmd{};
  if (next_command_ == 0) {
    cmd.funct = u32(static_cast<std::uint32_t>(smesh::SmeshFunct::Config));
    cmd.rs1 = u64(smesh::packConfig(smesh::ConfigKind::Load, 0, kLoadBlockStride));
    cmd.rs2 = u64(kDramRowStride);
  } else {
    cmd.funct = u32(static_cast<std::uint32_t>(smesh::SmeshFunct::Mvin));
    cmd.rs1 = u64(kDramBase);
    cmd.rs2 = u64(smesh::packLocal(smesh::makeAccAddr(0), shape));
  }

  cmd_bits = cmd;
  cmd_valid = 1;
  if (cmd_ready != 0) {
    trace("top_acc_load_driver: pushed funct=%u", static_cast<unsigned>(cmd.funct));
    ++next_command_;
  }
}

void TopAccLoadDriver::reset() {
  next_command_ = 0;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  TopAccLoadDriver driver("Driver");
  smesh::SmeshTop top("SmeshTop");
  smem::MemCtrl mem("MemCtrl");
  smem::Dram dram("Dram", 0);

  top.cmd_valid << driver.cmd_valid;
  top.cmd_bits << driver.cmd_bits;
  driver.cmd_ready << top.cmd_ready;
  mem.in_core_req << top.memReq();
  top.memResp() << mem.out_core_resp;
  mem.in_core_req.setDelay(1);
  dram.s_req << mem.s_req;
  mem.s_resp << dram.s_resp;

  Clock clk;
  driver.clk << clk;
  top.clk << clk;
  mem.clk << clk;
  dram.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();

  const std::array<std::uint8_t, smesh::kDim * smesh::kDim> rows{{
      0x01, 0x02, 0x03, 0x04,
      0x11, 0x12, 0x13, 0x14,
      0x21, 0x22, 0x23, 0x24,
      0x31, 0x32, 0x33, 0x34,
  }};
  for (std::size_t r = 0; r < smesh::kDim; ++r) {
    dram.write(kDramBase + r * kDramRowStride,
               rows.data() + r * smesh::kDim,
               smesh::kDim);
  }

  for (int i = 0; i < 128 && !(top.ldCtrl().hasDmaResponse() && top.rs().empty()); ++i) {
    Sim::run();
  }

  bool accum_ok = top.accum().hasAcceptedWrite();
  for (std::size_t r = 0; r < smesh::kDim; ++r) {
    const auto& acc_row = top.accum().row(smesh::makeAccAddr(static_cast<std::uint32_t>(r)));
    for (std::size_t c = 0; c < smesh::kDim; ++c) {
      accum_ok = accum_ok &&
                 acc_row[c] == static_cast<smesh::Acc>(rows[r * smesh::kDim + c]);
    }
  }

  const bool completion_ok = top.ldCtrl().hasDmaResponse() &&
                             top.ldCtrl().expectedBytes() == smesh::kDim * smesh::kDim &&
                             top.ldCtrl().returnedBytes() == smesh::kDim * smesh::kDim &&
                             top.ldCtrl().responseRsTag() == 1 &&
                             top.rs().empty();
  const bool ok = accum_ok && completion_ok;
  if (!ok) {
    const auto& load0 = top.rs().loadEntry(0);
    std::printf("  accum_ok=%u accepted_write=%u\n",
                accum_ok ? 1u : 0u,
                top.accum().hasAcceptedWrite() ? 1u : 0u);
    std::printf("  load0 valid=%u issued=%u ready=%u funct=%u tag=%u deps_ld=0x%x\n",
                load0.valid ? 1u : 0u,
                load0.issued ? 1u : 0u,
                load0.ready() ? 1u : 0u,
                static_cast<unsigned>(load0.cmd.funct),
                static_cast<unsigned>(load0.rs_tag),
                load0.deps_ld);
    std::printf("  completion_ok=%u has_dma_resp=%u expected=%u returned=%u tag=%u rs_empty=%u\n",
                completion_ok ? 1u : 0u,
                top.ldCtrl().hasDmaResponse() ? 1u : 0u,
                top.ldCtrl().expectedBytes(),
                top.ldCtrl().returnedBytes(),
                static_cast<unsigned>(top.ldCtrl().responseRsTag()),
                top.rs().empty() ? 1u : 0u);
  }
  std::printf("[SMESH_TOP_ACC_LOAD] %s top_level_mvin_to_accum\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
