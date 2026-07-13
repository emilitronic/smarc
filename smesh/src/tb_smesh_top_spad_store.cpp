// **********************************************************************
// smesh/src/tb_smesh_top_spad_store.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
// Focused SmeshTop store-path test from scratchpad into the store data path.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "SmeshCommand.hpp"
#include "SmeshTop.hpp"
#include "smem/Dram.hpp"
#include "smem/MemCtrl.hpp"

#include <array>
#include <cstdio>

constexpr std::uint64_t kLoadDramBase = 0x80006000;
constexpr std::uint64_t kStoreDramBase = 0x80007000;
constexpr std::uint32_t kDramRowStride = 9;
constexpr std::uint32_t kLoadBlockStride = 5;

class TopSpadStoreDriver : public Component {
  DECLARE_COMPONENT(TopSpadStoreDriver);

 public:
  TopSpadStoreDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, cmd_valid);
  Output(smesh::SmeshCmd, cmd_bits);
  Input(bit, cmd_ready);

  void update();
  void reset();

 private:
  std::uint32_t next_command_ = 0;
};

TopSpadStoreDriver::TopSpadStoreDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cmd_ready).writes(cmd_valid, cmd_bits);
}

void TopSpadStoreDriver::update() {
  cmd_valid = 0;
  if (next_command_ >= 3) {
    return;
  }

  constexpr smesh::MatrixShape shape{smesh::kDim, smesh::kDim};
  smesh::SmeshCmd cmd{};
  if (next_command_ == 0) {
    cmd.funct = u32(static_cast<std::uint32_t>(smesh::SmeshFunct::Config));
    cmd.rs1 = u64(smesh::packConfig(smesh::ConfigKind::Load, 0, kLoadBlockStride));
    cmd.rs2 = u64(kDramRowStride);
  } else if (next_command_ == 1) {
    cmd.funct = u32(static_cast<std::uint32_t>(smesh::SmeshFunct::Mvin));
    cmd.rs1 = u64(kLoadDramBase);
    cmd.rs2 = u64(smesh::packLocal(smesh::makeSpAddr(0), shape));
  } else {
    cmd.funct = u32(static_cast<std::uint32_t>(smesh::SmeshFunct::Mvout));
    cmd.rs1 = u64(kStoreDramBase);
    cmd.rs2 = u64(smesh::packLocal(smesh::makeSpAddr(0), shape));
  }

  cmd_bits = cmd;
  cmd_valid = 1;
  if (cmd_ready != 0) {
    trace("top_spad_store_driver: pushed funct=%u", static_cast<unsigned>(cmd.funct));
    ++next_command_;
  }
}

void TopSpadStoreDriver::reset() {
  next_command_ = 0;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  TopSpadStoreDriver driver("Driver");
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

  Sim::init();
  Sim::reset();

  const std::array<std::uint8_t, smesh::kDim * smesh::kDim> rows{{
      0x01, 0x02, 0x03, 0x04,
      0x11, 0x12, 0x13, 0x14,
      0x21, 0x22, 0x23, 0x24,
      0x31, 0x32, 0x33, 0x34,
  }};
  for (std::size_t r = 0; r < smesh::kDim; ++r) {
    dram.write(kLoadDramBase + r * kDramRowStride,
               rows.data() + r * smesh::kDim,
               smesh::kDim);
  }

  for (int i = 0; i < 192 && !top.spadDmaReadPipe().hasAcceptedResponse(); ++i) {
    Sim::run();
  }

  bool spad_ok = top.spad().hasAcceptedWrite();
  for (std::size_t c = 0; c < smesh::kDim; ++c) {
    const auto& row0 = top.spad().row(smesh::makeSpAddr(0));
    spad_ok = spad_ok && row0[c] == static_cast<smesh::Elem>(rows[c]);
  }

  bool pipe_ok = top.spadDmaReadPipe().hasAcceptedResponse();
  const auto& resp = top.spadDmaReadPipe().lastResponse();
  pipe_ok = pipe_ok &&
            resp.laddr.full_sp_addr() == 0 &&
            resp.len == smesh::kDim &&
            resp.mask == ((1u << smesh::kDim) - 1u);
  for (std::size_t c = 0; c < smesh::kDim; ++c) {
    const auto byte = static_cast<std::uint8_t>((resp.data >> (c * 8)) & 0xffu);
    pipe_ok = pipe_ok && byte == rows[c];
  }

  const bool ok = spad_ok && pipe_ok;
  if (!ok) {
    const auto& store0 = top.rs().storeEntry(0);
    std::printf("  spad_ok=%u pipe_ok=%u accepted_resp=%u\n",
                spad_ok ? 1u : 0u,
                pipe_ok ? 1u : 0u,
                top.spadDmaReadPipe().hasAcceptedResponse() ? 1u : 0u);
    std::printf("  store0 valid=%u issued=%u ready=%u funct=%u tag=%u deps_ld=0x%x deps_st=0x%x\n",
                store0.valid ? 1u : 0u,
                store0.issued ? 1u : 0u,
                store0.ready() ? 1u : 0u,
                static_cast<unsigned>(store0.cmd.funct),
                static_cast<unsigned>(store0.rs_tag),
                store0.deps_ld,
                store0.deps_st);
    std::printf("  resp laddr=0x%x len=%u mask=0x%x data=0x%llx\n",
                static_cast<unsigned>(resp.laddr.raw),
                static_cast<unsigned>(resp.len),
                static_cast<unsigned>(resp.mask),
                static_cast<unsigned long long>(resp.data));
  }

  std::printf("[SMESH_TOP_SPAD_STORE] %s spad_store_read_path\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
