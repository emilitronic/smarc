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

// Testbench-only checker: verifies that the store-path local read request
// and matching metadata enqueue fire in the same cycle with matching fields.
class StorePathMonitor : public Component {
  DECLARE_COMPONENT(StorePathMonitor);

 public:
  StorePathMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, spad_req_val);
  Input(bit, spad_req_rdy);
  Input(smesh::SpadReadReq, spad_req_bits);
  Input(bit, norm_enq_val);
  Input(bit, norm_enq_rdy);
  Input(smesh::DmaWriteReq, norm_enq_bits);
  Input(bit, dma_writer_req_val);
  Input(bit, dma_writer_req_rdy);
  Input(smesh::StWriterReq, dma_writer_req_bits);

  void update();
  void updateWriter();
  void reset();

  bool sawAlignedTransfer() const { return saw_aligned_transfer_; }
  bool sawDmaWriterTransfer() const { return saw_dma_writer_transfer_; }
  std::uint32_t alignedTransferCount() const { return aligned_transfer_count_; }

 private:
  bool saw_aligned_transfer_ = false;
  bool saw_dma_writer_transfer_ = false;
  std::uint32_t aligned_transfer_count_ = 0;
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

StorePathMonitor::StorePathMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(spad_req_val,
                       spad_req_rdy,
                       spad_req_bits,
                       norm_enq_val,
                       norm_enq_rdy,
                       norm_enq_bits);
  UPDATE(updateWriter).reads(
                       dma_writer_req_val,
                       dma_writer_req_rdy,
                       dma_writer_req_bits);
}

void StorePathMonitor::update() {
  const bool spad_fire = spad_req_val != 0 && spad_req_rdy != 0;
  const bool norm_fire = norm_enq_val != 0 && norm_enq_rdy != 0;
  if (!spad_fire && !norm_fire) {
    return;
  }

  if (spad_fire || norm_fire) {
    assert_always(spad_fire == norm_fire,
                  "store monitor saw spad read request and norm enqueue move in different cycles");

    const auto spad_req = *spad_req_bits;
    const auto norm_req = *norm_enq_bits;
    assert_always(spad_req.laddr.raw == norm_req.laddr.raw,
                  "store monitor saw mismatched local addresses");
    assert_always(spad_req.len == norm_req.len,
                  "store monitor saw mismatched lengths");
    assert_always(spad_req.cmd_id == norm_req.cmd_id,
                  "store monitor saw mismatched command IDs");

    saw_aligned_transfer_ = true;
    ++aligned_transfer_count_;
    trace("store_path_monitor: aligned spad/norm laddr=0x%x len=%u cmd_id=%u",
          static_cast<unsigned>(spad_req.laddr.raw),
          static_cast<unsigned>(spad_req.len),
          static_cast<unsigned>(spad_req.cmd_id));
  }
}

void StorePathMonitor::updateWriter() {
  const bool dma_writer_fire = dma_writer_req_val != 0 && dma_writer_req_rdy != 0;
  if (!dma_writer_fire) {
    return;
  }
  const auto writer_req = *dma_writer_req_bits;
  assert_always(writer_req.issue.vaddr == kStoreDramBase,
                "store monitor saw wrong DMA writer address");
  assert_always(writer_req.issue.dest == 0,
                "store monitor expected normal DMA writer destination");
  assert_always(writer_req.len_bytes == smesh::kDim * sizeof(smesh::Elem),
                "store monitor saw wrong DMA writer byte count");
  for (std::size_t i = 0; i < smesh::kDim; ++i) {
    assert_always(writer_req.data[i] == static_cast<std::uint8_t>(0x01 + i),
                  "store monitor saw wrong DMA writer data byte");
  }

  saw_dma_writer_transfer_ = true;
}

void StorePathMonitor::reset() {
  saw_aligned_transfer_ = false;
  saw_dma_writer_transfer_ = false;
  aligned_transfer_count_ = 0;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  TopSpadStoreDriver driver("Driver");
  StorePathMonitor monitor("StorePathMonitor");
  smesh::SmeshTop top("SmeshTop");
  smem::MemCtrl mem("MemCtrl");
  smem::Dram dram("Dram", 0);

  top.cmd_valid << driver.cmd_valid;
  top.cmd_bits << driver.cmd_bits;
  driver.cmd_ready << top.cmd_ready;
  mem.in_core_req << top.memReq();
  top.memResp() << mem.out_core_resp;
  monitor.spad_req_val << top.storeSpadReadReqVal();
  monitor.spad_req_rdy << top.storeSpadReadReqRdy();
  monitor.spad_req_bits << top.storeSpadReadReqBits();
  monitor.norm_enq_val << top.storeNormEnqVal();
  monitor.norm_enq_rdy << top.storeNormEnqRdy();
  monitor.norm_enq_bits << top.storeNormEnqBits();
  monitor.dma_writer_req_val << top.storeDmaWriterReqVal();
  monitor.dma_writer_req_rdy << top.storeDmaWriterReqRdy();
  monitor.dma_writer_req_bits << top.storeDmaWriterReqBits();
  mem.in_core_req.setDelay(1);
  dram.s_req << mem.s_req;
  mem.s_resp << dram.s_resp;

  Clock clk;
  driver.clk << clk;
  monitor.clk << clk;
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

  for (int i = 0; i < 192 && !monitor.sawDmaWriterTransfer(); ++i) {
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
    const auto byte = resp.data[c];
    pipe_ok = pipe_ok && byte == rows[c];
  }

  const bool monitor_ok = monitor.sawAlignedTransfer() &&
                          monitor.sawDmaWriterTransfer() &&
                          monitor.alignedTransferCount() == 1;
  const bool ok = spad_ok && pipe_ok && monitor_ok;
  if (!ok) {
    const auto& store0 = top.rs().storeEntry(0);
    std::printf("  spad_ok=%u pipe_ok=%u monitor_ok=%u accepted_resp=%u writer_seen=%u aligned_count=%u\n",
                spad_ok ? 1u : 0u,
                pipe_ok ? 1u : 0u,
                monitor_ok ? 1u : 0u,
                top.spadDmaReadPipe().hasAcceptedResponse() ? 1u : 0u,
                monitor.sawDmaWriterTransfer() ? 1u : 0u,
                monitor.alignedTransferCount());
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
                static_cast<unsigned long long>(smesh::low64DmaReadData(resp.data)));
  }

  std::printf("[SMESH_TOP_SPAD_STORE] %s spad_store_read_path\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
