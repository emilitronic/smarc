// **********************************************************************
// smesh/tests/unit/memory/tb_arb_write_spad.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 29 2026
/*
Focused test to cover signal conversion between ex_ctrl local write and ArbWriteSpad
and ArbWriteAccum write to Spad and Accum, respectively. 
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ArbWriteLocal.hpp"

#include <cstdio>

namespace {

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, one);
  Output(bit, zero);
  Output(smesh::SpadBankWriteReq, exwrite_bits);
  Output(smesh::DmaReadResp,      dma_bits);
  Input(bit, exwrite_rdy);
  Input(bit, dmaread_rdy);
  Input(bit, write_val);
  Input(smesh::DmaReadResp, write_bits);

  void updateDrive();
  void updateCheck();
  void reset();
  bool passed() const { return passed_; }

 private:
  bool passed_ = true;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateDrive).writes(one, zero, exwrite_bits, dma_bits);
  UPDATE(updateCheck)
      .reads(exwrite_rdy, dmaread_rdy, write_val, write_bits);
}

void Driver::updateDrive() {
  one  = 1;
  zero = 0;
  smesh::SpadBankWriteReq req{};
  req.addr = 3;
  req.mask = 0xabcdef12u;
  for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
    req.data[lane] = static_cast<smesh::Elem>(10 + lane);
  }
  exwrite_bits = req;
  dma_bits = smesh::DmaReadResp{};
}

void Driver::updateCheck() {
  bool ok = write_val == 1 && exwrite_rdy == 1 && dmaread_rdy == 0;
  const auto write = *write_bits;
  ok &= write.laddr.sp_bank() == 1 && write.laddr.sp_row() == 3;
  ok &= write.mask == 0xabcdef12u;
  for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
    ok &= write.data[lane] == 10 + lane;
  }
  passed_ &= ok;
}

void Driver::reset() {
  passed_ = true;
  one.reset(1);
  zero.reset(0);
  exwrite_bits.reset(smesh::SpadBankWriteReq{});
  dma_bits.reset(smesh::DmaReadResp{});
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  Driver driver("Driver");
  smesh::ArbWriteSpad arb("ArbWriteSpad", 1);
  arb.exwrite_val    << driver.one;
  arb.exwrite_bits   << driver.exwrite_bits;
  driver.exwrite_rdy << arb.exwrite_rdy;
  arb.dmaread_val    << driver.one;
  arb.dmaread_bits   << driver.dma_bits;
  driver.dmaread_rdy << arb.dmaread_rdy;
  arb.zerowrite_val  << driver.zero;
  arb.zerowrite_bits << driver.dma_bits;
  arb.write_rdy      << driver.one;
  driver.write_val   << arb.write_val;
  driver.write_bits  << arb.write_bits;

  Clock clk;
  driver.clk << clk;
  arb.clk    << clk;
  clk.generateClock();

  Sim::init();
  Sim::reset();
  Sim::run();
  std::printf("[ARB_WRITE_SPAD] %s\n", driver.passed() ? "PASS" : "FAIL");
  return driver.passed() ? 0 : 1;
}
