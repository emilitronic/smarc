// **********************************************************************
// smesh/tests/unit/st_ctrl/tb_st_ctrl_dma_req.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
// Focused store-controller DMA request payload test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "StCtrlDmaReq.hpp"

#include <cstdio>

namespace {

constexpr std::uint32_t kNormSum       = 1;
constexpr std::uint32_t kNormMean      = 2;
constexpr std::uint32_t kNormInvStddev = 4;

class DmaReqDriver : public Component {
  DECLARE_COMPONENT(DmaReqDriver);

 public:
  DmaReqDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, dst_is_spad);
  Output(u32, cols);
  Output(u32, blocks);
  Output(u32, mstatus);
  Output(bit, pooling_is_enabled);
  Output(bit, mvout_1d_enabled);
  Output(u64, current_vaddr);
  Output(smesh::SmeshLocalAddr, current_localaddr);
  Output(u64, current_dst_spad_addr);
  Output(smesh::SmeshLocalAddr, pool_row_addr);
  Output(u64, pool_vaddr);
  Output(u8, activation);
  Output(u32, acc_scale);
  Output(u32, igelu_qb);
  Output(u32, igelu_qc);
  Output(u32, iexp_qln2);
  Output(u32, iexp_qln2_inv);
  Output(u16, norm_stats_id);
  Output(u32, block_counter);
  Output(u32, wrow_counter);
  Output(u32, wcol_counter);
  Output(u8, pool_size);
  Output(u16, cmd_id);

  void update();
  void reset();

 private:
  unsigned cycle_ = 0;
};

class DmaReqMonitor : public Component {
  DECLARE_COMPONENT(DmaReqMonitor);

 public:
  DmaReqMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(smesh::DmaWriteReq, req_bits);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  unsigned cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

DmaReqDriver::DmaReqDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(dst_is_spad,
              cols,
              blocks,
              mstatus,
              pooling_is_enabled,
              mvout_1d_enabled,
              current_vaddr,
              current_localaddr)
      .writes(current_dst_spad_addr,
              pool_row_addr,
              pool_vaddr,
              activation,
              acc_scale,
              igelu_qb,
              igelu_qc,
              iexp_qln2)
      .writes(iexp_qln2_inv,
              norm_stats_id,
              block_counter,
              wrow_counter,
              wcol_counter,
              pool_size,
              cmd_id);
}

void DmaReqDriver::update() {
  dst_is_spad = 0;
  cols = 6;
  blocks = 2;
  mstatus = 0x12345678u;
  pooling_is_enabled = 0;
  mvout_1d_enabled = 0;
  current_vaddr = 0x1000;
  current_localaddr = smesh::makeAccAddr(12, false, false, kNormMean);
  current_dst_spad_addr = 0x2000;
  pool_row_addr = smesh::makeAccAddr(7);
  pool_vaddr = 0x3000;
  activation = 2;
  acc_scale = 0x11112222u;
  igelu_qb = 0x33334444u;
  igelu_qc = 0x55556666u;
  iexp_qln2 = 0x77778888u;
  iexp_qln2_inv = 0x9999aaaau;
  norm_stats_id = 0x55;
  block_counter = 0;
  wrow_counter = 0;
  wcol_counter = 0;
  pool_size = 2;
  cmd_id = 9;

  if (cycle_ == 1) {
    block_counter = 1;
  } else if (cycle_ == 2 || cycle_ == 3) {
    cols = 4;
    blocks = 1;
    current_localaddr = smesh::makeAccAddr(12, false, false, kNormInvStddev);
    pooling_is_enabled = 1;
    wrow_counter = cycle_ == 3 ? 1 : 0;
    wcol_counter = 1;
  } else if (cycle_ == 4) {
    dst_is_spad = 1;
    cols = 3;
    blocks = 1;
    mvout_1d_enabled = 1;
  } else if (cycle_ == 5) {
    cols = 3;
    blocks = 1;
    mvout_1d_enabled = 1;
  }

  ++cycle_;
}

void DmaReqDriver::reset() {
  cycle_ = 0;
  dst_is_spad.reset(0);
  cols.reset(0);
  blocks.reset(0);
  mstatus.reset(0);
  pooling_is_enabled.reset(0);
  mvout_1d_enabled.reset(0);
  current_vaddr.reset(0);
  current_localaddr.reset(smesh::SmeshLocalAddr{});
  current_dst_spad_addr.reset(0);
  pool_row_addr.reset(smesh::SmeshLocalAddr{});
  pool_vaddr.reset(0);
  activation.reset(0);
  acc_scale.reset(0);
  igelu_qb.reset(0);
  igelu_qc.reset(0);
  iexp_qln2.reset(0);
  iexp_qln2_inv.reset(0);
  norm_stats_id.reset(0);
  block_counter.reset(0);
  wrow_counter.reset(0);
  wcol_counter.reset(0);
  pool_size.reset(0);
  cmd_id.reset(0);
}

DmaReqMonitor::DmaReqMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(req_bits);
}

void DmaReqMonitor::update() {
  if (Sim::state == Sim::SimResetting || done_) {
    return;
  }

  const auto req = *req_bits;
  bool ok = req.acc_act == 2 && req.acc_scale == 0x11112222u &&
            req.acc_igelu_qb == 0x33334444u &&
            req.acc_igelu_qc == 0x55556666u &&
            req.acc_iexp_qln2 == 0x77778888u &&
            req.acc_iexp_qln2_inv == 0x9999aaaau &&
            req.acc_norm_stats_id == 0x55 && req.cmd_id == 9 &&
            req.status == 0x12345678u;

  if (cycle_ == 0) {
    ok = ok && req.vaddr == 0x1000 && req.dest == 0 && req.len == smesh::kDim &&
         req.block == 0 && req.laddr.data() == 12 &&
         req.laddr.norm_cmd() == kNormSum && req.pool_en == 0 && req.store_en == 0;
  } else if (cycle_ == 1) {
    ok = ok && req.vaddr == 0x1000 && req.len == 2 && req.block == 1 &&
         req.laddr.norm_cmd() == kNormMean && req.store_en != 0;
  } else if (cycle_ == 2) {
    ok = ok && req.vaddr == 0x3000 && req.laddr.data() == 7 &&
         req.laddr.norm_cmd() == kNormInvStddev && req.pool_en != 0 &&
         req.store_en == 0;
  } else if (cycle_ == 3) {
    ok = ok && req.vaddr == 0x3000 && req.laddr.data() == 7 &&
         req.pool_en != 0 && req.store_en != 0;
  } else if (cycle_ == 4) {
    ok = ok && req.vaddr == 0x2000 && req.dest == 1 && req.len == 3 &&
         req.laddr.data() == 12 && req.store_en != 0;
  } else {
    ok = ok && req.vaddr == 0x3000 && req.dest == 0 && req.len == 3 &&
         req.laddr.data() == 12 && req.store_en != 0;
  }

  if (!ok) {
    std::printf("[ST_CTRL_DMA_REQ] mismatch case=%u vaddr=0x%llx laddr=0x%x dest=%u len=%u block=%u pool=%u store=%u\n",
                cycle_,
                static_cast<unsigned long long>(req.vaddr),
                req.laddr.raw,
                static_cast<unsigned>(req.dest),
                static_cast<unsigned>(req.len),
                static_cast<unsigned>(req.block),
                static_cast<unsigned>(req.pool_en != 0),
                static_cast<unsigned>(req.store_en != 0));
  }

  passed_ = passed_ && ok;
  ++cycle_;
  done_ = cycle_ == 6;
}

void DmaReqMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  DmaReqDriver driver("Driver");
  smesh::StCtrlDmaReq dma_req("StCtrlDmaReq");
  DmaReqMonitor monitor("Monitor");

  dma_req.dst_is_spad << driver.dst_is_spad;
  dma_req.cols << driver.cols;
  dma_req.blocks << driver.blocks;
  dma_req.mstatus << driver.mstatus;
  dma_req.pooling_is_enabled << driver.pooling_is_enabled;
  dma_req.mvout_1d_enabled << driver.mvout_1d_enabled;
  dma_req.current_vaddr << driver.current_vaddr;
  dma_req.current_localaddr << driver.current_localaddr;
  dma_req.current_dst_spad_addr << driver.current_dst_spad_addr;
  dma_req.pool_row_addr << driver.pool_row_addr;
  dma_req.pool_vaddr << driver.pool_vaddr;
  dma_req.activation << driver.activation;
  dma_req.acc_scale << driver.acc_scale;
  dma_req.igelu_qb << driver.igelu_qb;
  dma_req.igelu_qc << driver.igelu_qc;
  dma_req.iexp_qln2 << driver.iexp_qln2;
  dma_req.iexp_qln2_inv << driver.iexp_qln2_inv;
  dma_req.norm_stats_id << driver.norm_stats_id;
  dma_req.block_counter << driver.block_counter;
  dma_req.wrow_counter << driver.wrow_counter;
  dma_req.wcol_counter << driver.wcol_counter;
  dma_req.pool_size << driver.pool_size;
  dma_req.cmd_id << driver.cmd_id;
  monitor.req_bits << dma_req.req_bits;

  Clock clk;
  driver.clk << clk;
  dma_req.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 8 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[ST_CTRL_DMA_REQ] %s payload_selection\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
