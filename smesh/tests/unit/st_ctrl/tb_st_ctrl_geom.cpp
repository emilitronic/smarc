// **********************************************************************
// smesh/tests/unit/st_ctrl/tb_st_ctrl_geom.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
// Focused store-controller pooling and moveout geometry test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "StCtrlGeom.hpp"

#include <cstdio>

class GeomDriver : public Component {
  DECLARE_COMPONENT(GeomDriver);

 public:
  GeomDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(u64, vaddr);
  Output(smesh::SmeshLocalAddr, localaddr);
  Output(smesh::SmeshLocalAddr, dst_spad_addr);
  Output(u32, dst_spad_stride);
  Output(u32, stride);
  Output(u8, pool_stride);
  Output(u8, pool_size);
  Output(u8, pool_out_dim);
  Output(u8, pool_porows);
  Output(u8, pool_pocols);
  Output(u8, pool_orows);
  Output(u8, pool_ocols);
  Output(u8, pool_upad);
  Output(u8, pool_lpad);
  Output(u32, row_counter);
  Output(u32, block_counter);
  Output(u32, porow_counter);
  Output(u32, pocol_counter);
  Output(u32, wrow_counter);
  Output(u32, wcol_counter);

  void update();
  void reset();

 private:
  unsigned cycle_ = 0;
};

class GeomMonitor : public Component {
  DECLARE_COMPONENT(GeomMonitor);

 public:
  GeomMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, pooling_is_enabled);
  Input(bit, mvout_1d_enabled);
  Input(u32, orow);
  Input(u32, ocol);
  Input(bit, orow_is_negative);
  Input(bit, ocol_is_negative);
  Input(u32, pool_total_rows);
  Input(u32, mvout_1d_rows);
  Input(u64, current_vaddr);
  Input(smesh::SmeshLocalAddr, current_localaddr);
  Input(u64, current_dst_spad_addr);
  Input(smesh::SmeshLocalAddr, pool_row_addr);
  Input(u64, pool_vaddr);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  unsigned cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

GeomDriver::GeomDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(vaddr,
              localaddr,
              dst_spad_addr,
              dst_spad_stride,
              stride,
              pool_stride,
              pool_size,
              pool_out_dim)
      .writes(pool_porows,
              pool_pocols,
              pool_orows,
              pool_ocols,
              pool_upad,
              pool_lpad,
              row_counter,
              block_counter)
      .writes(porow_counter,
              pocol_counter,
              wrow_counter,
              wcol_counter);
}

void GeomDriver::update() {
  vaddr = cycle_ == 0 ? 0x1000 : 0x2000;
  localaddr = smesh::makeSpAddr(cycle_ == 0 ? 10 : 4);
  dst_spad_addr = smesh::makeSpAddr(20);
  dst_spad_stride = 3;
  stride = cycle_ == 0 ? 16 : 8;
  pool_stride = 0;
  pool_size = 0;
  pool_out_dim = 0;
  pool_porows = 0;
  pool_pocols = 0;
  pool_orows = 0;
  pool_ocols = 0;
  pool_upad = 0;
  pool_lpad = 0;
  row_counter = 0;
  block_counter = 0;
  porow_counter = 0;
  pocol_counter = 0;
  wrow_counter = 0;
  wcol_counter = 0;

  if (cycle_ == 0) {
    row_counter = 2;
    block_counter = 1;
  } else if (cycle_ == 1) {
    pool_stride = 2;
    pool_size = 2;
    pool_out_dim = 3;
    pool_porows = 2;
    pool_pocols = 3;
    pool_orows = 5;
    pool_ocols = 6;
    pool_upad = 1;
    pool_lpad = 1;
    porow_counter = 1;
    pocol_counter = 2;
    wcol_counter = 1;
  } else if (cycle_ == 2) {
    pool_stride = 2;
    pool_size = 2;
    pool_out_dim = 3;
    pool_porows = 2;
    pool_pocols = 3;
    pool_orows = 5;
    pool_ocols = 6;
    pool_upad = 1;
    pool_lpad = 1;
  } else {
    pool_size = 2;
    pool_out_dim = 3;
    pool_orows = 3;
    pool_ocols = 5;
  }

  ++cycle_;
}

void GeomDriver::reset() {
  cycle_ = 0;
  vaddr.reset(0);
  localaddr.reset(smesh::SmeshLocalAddr{});
  dst_spad_addr.reset(smesh::SmeshLocalAddr{});
  dst_spad_stride.reset(0);
  stride.reset(0);
  pool_stride.reset(0);
  pool_size.reset(0);
  pool_out_dim.reset(0);
  pool_porows.reset(0);
  pool_pocols.reset(0);
  pool_orows.reset(0);
  pool_ocols.reset(0);
  pool_upad.reset(0);
  pool_lpad.reset(0);
  row_counter.reset(0);
  block_counter.reset(0);
  porow_counter.reset(0);
  pocol_counter.reset(0);
  wrow_counter.reset(0);
  wcol_counter.reset(0);
}

GeomMonitor::GeomMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(pooling_is_enabled,
             mvout_1d_enabled,
             orow,
             ocol,
             orow_is_negative,
             ocol_is_negative,
             pool_total_rows,
             mvout_1d_rows)
      .reads(current_vaddr,
             current_localaddr,
             current_dst_spad_addr,
             pool_row_addr,
             pool_vaddr);
}

void GeomMonitor::update() {
  if (Sim::state == Sim::SimResetting || done_) {
    return;
  }

  bool ok = false;
  if (cycle_ == 0) {
    ok = pooling_is_enabled == 0 && mvout_1d_enabled == 0 &&
         *current_vaddr == 0x1020 &&
         current_localaddr->raw == smesh::makeSpAddr(16).raw &&
         *current_dst_spad_addr == 10;
  } else if (cycle_ == 1) {
    ok = pooling_is_enabled == 1 && mvout_1d_enabled == 0 &&
         *orow == 1 && *ocol == 4 &&
         orow_is_negative == 0 && ocol_is_negative == 0 &&
         *pool_total_rows == 24 &&
         pool_row_addr->raw == smesh::makeSpAddr(14).raw &&
         *pool_vaddr == 0x2028;
  } else if (cycle_ == 2) {
    ok = pooling_is_enabled == 1 &&
         orow_is_negative == 1 && ocol_is_negative == 1 &&
         pool_row_addr->is_garbage();
  } else {
    ok = pooling_is_enabled == 0 && mvout_1d_enabled == 1 &&
         *mvout_1d_rows == 15;
  }

  if (!ok) {
    std::printf("[ST_CTRL_GEOM] mismatch case=%u pool=%u one_d=%u orow=%u ocol=%u neg=%u%u total=%u one_d_rows=%u cvaddr=0x%llx claddr=0x%x dst=0x%llx prow=0x%x pvaddr=0x%llx\n",
                cycle_,
                static_cast<unsigned>(pooling_is_enabled == 1),
                static_cast<unsigned>(mvout_1d_enabled == 1),
                static_cast<unsigned>(*orow),
                static_cast<unsigned>(*ocol),
                static_cast<unsigned>(orow_is_negative == 1),
                static_cast<unsigned>(ocol_is_negative == 1),
                static_cast<unsigned>(*pool_total_rows),
                static_cast<unsigned>(*mvout_1d_rows),
                static_cast<unsigned long long>(*current_vaddr),
                static_cast<unsigned>(current_localaddr->raw),
                static_cast<unsigned long long>(*current_dst_spad_addr),
                static_cast<unsigned>(pool_row_addr->raw),
                static_cast<unsigned long long>(*pool_vaddr));
  }

  passed_ = passed_ && ok;
  ++cycle_;
  done_ = cycle_ == 4;
}

void GeomMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = true;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  GeomDriver driver("Driver");
  smesh::StCtrlGeom geometry("StCtrlGeom");
  GeomMonitor monitor("Monitor");

  geometry.vaddr << driver.vaddr;
  geometry.localaddr << driver.localaddr;
  geometry.dst_spad_addr << driver.dst_spad_addr;
  geometry.dst_spad_stride << driver.dst_spad_stride;
  geometry.stride << driver.stride;
  geometry.pool_stride << driver.pool_stride;
  geometry.pool_size << driver.pool_size;
  geometry.pool_out_dim << driver.pool_out_dim;
  geometry.pool_porows << driver.pool_porows;
  geometry.pool_pocols << driver.pool_pocols;
  geometry.pool_orows << driver.pool_orows;
  geometry.pool_ocols << driver.pool_ocols;
  geometry.pool_upad << driver.pool_upad;
  geometry.pool_lpad << driver.pool_lpad;
  geometry.row_counter << driver.row_counter;
  geometry.block_counter << driver.block_counter;
  geometry.porow_counter << driver.porow_counter;
  geometry.pocol_counter << driver.pocol_counter;
  geometry.wrow_counter << driver.wrow_counter;
  geometry.wcol_counter << driver.wcol_counter;

  monitor.pooling_is_enabled << geometry.pooling_is_enabled;
  monitor.mvout_1d_enabled << geometry.mvout_1d_enabled;
  monitor.orow << geometry.orow;
  monitor.ocol << geometry.ocol;
  monitor.orow_is_negative << geometry.orow_is_negative;
  monitor.ocol_is_negative << geometry.ocol_is_negative;
  monitor.pool_total_rows << geometry.pool_total_rows;
  monitor.mvout_1d_rows << geometry.mvout_1d_rows;
  monitor.current_vaddr << geometry.current_vaddr;
  monitor.current_localaddr << geometry.current_localaddr;
  monitor.current_dst_spad_addr << geometry.current_dst_spad_addr;
  monitor.pool_row_addr << geometry.pool_row_addr;
  monitor.pool_vaddr << geometry.pool_vaddr;

  Clock clk;
  driver.clk << clk;
  geometry.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 6 && !monitor.done(); ++i) {
    Sim::run();
  }

  descore::flushLog();
  const bool ok = monitor.done() && monitor.passed();
  std::printf("[ST_CTRL_GEOM] %s geometry\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
