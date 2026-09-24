// **********************************************************************
// smesh/tests/unit/ld_ctrl/tb_ld_ctrl_geom.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026
// Focused test covers normal progression, zero stride, zero address, and address wrapping.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LdCtrlGeom.hpp"

#include <cstdio>

namespace {

class GeomDriver : public Component {
  DECLARE_COMPONENT(GeomDriver);

 public:
  GeomDriver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(u64, vaddr);
  Output(smesh::SmeshLocalAddr, localaddr);
  Output(u32, rows);
  Output(u64, stride);
  Output(u32, row_counter);
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
  Input(bit, all_zeros);
  Input(u64, current_vaddr);
  Input(smesh::SmeshLocalAddr, localaddr_plus_row_counter);
  Input(u32, actual_rows_read);
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
  UPDATE(update).writes(vaddr, localaddr, rows, stride, row_counter);
}

void GeomDriver::update() {
  switch (cycle_) {
    case 0:
      vaddr = 0x1000;
      localaddr = smesh::makeSpAddr(5);
      rows = 4;
      stride = 16;
      row_counter = 2;
      break;
    case 1:
      vaddr = 0x2000;
      localaddr = smesh::makeAccAddr(7, true, true, 1);
      rows = 4;
      stride = 0;
      row_counter = 3;
      break;
    case 2:
      vaddr = 0;
      localaddr = smesh::makeSpAddr(9);
      rows = 4;
      stride = 0;
      row_counter = 3;
      break;
    case 3:
      vaddr = 0x3000;
      localaddr = smesh::makeSpAddr(smesh::kSpAddrMask);
      rows = 2;
      stride = 4;
      row_counter = 1;
      break;
    default:
      vaddr = 0xfffffffffffffff8ull;
      localaddr = smesh::makeSpAddr(0);
      rows = 3;
      stride = 8;
      row_counter = 2;
      break;
  }
  ++cycle_;
}

void GeomDriver::reset() {
  cycle_ = 0;
  vaddr.reset(0);
  localaddr.reset(smesh::SmeshLocalAddr{});
  rows.reset(0);
  stride.reset(0);
  row_counter.reset(0);
}

GeomMonitor::GeomMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(all_zeros, current_vaddr, localaddr_plus_row_counter,
                       actual_rows_read);
}

void GeomMonitor::update() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  const std::uint64_t expected_vaddr[] = {0x1020, 0x2000, 0, 0x3004, 8};
  const auto expected_local = cycle_ == 0 ? smesh::makeSpAddr(7) :
                              cycle_ == 1 ? smesh::makeAccAddr(10, true, true, 1) :
                              cycle_ == 2 ? smesh::makeSpAddr(12) :
                              cycle_ == 3 ? smesh::makeSpAddr(0) :
                                            smesh::makeSpAddr(2);
  const unsigned expected_rows[] = {4, 1, 4, 2, 3};
  passed_ &= (all_zeros == 1) == (cycle_ == 2);
  passed_ &= static_cast<std::uint64_t>(current_vaddr) == expected_vaddr[cycle_];
  passed_ &= (*localaddr_plus_row_counter).raw == expected_local.raw;
  passed_ &= static_cast<unsigned>(actual_rows_read) == expected_rows[cycle_];

  std::printf("[c%u] zero=%u vaddr=%016llx laddr=%08x rows=%u\n",
              cycle_, static_cast<unsigned>(all_zeros == 1),
              static_cast<unsigned long long>(current_vaddr),
              (*localaddr_plus_row_counter).raw,
              static_cast<unsigned>(actual_rows_read));
  done_ = cycle_ == 4;
  ++cycle_;
}

void GeomMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::LdCtrlGeom geom("LoadGeom");
  GeomDriver driver("Driver");
  GeomMonitor monitor("Monitor");
  geom.vaddr << driver.vaddr;
  geom.localaddr << driver.localaddr;
  geom.rows << driver.rows;
  geom.stride << driver.stride;
  geom.row_counter << driver.row_counter;
  monitor.all_zeros << geom.all_zeros;
  monitor.current_vaddr << geom.current_vaddr;
  monitor.localaddr_plus_row_counter << geom.localaddr_plus_row_counter;
  monitor.actual_rows_read << geom.actual_rows_read;

  Clock clk;
  geom.clk << clk;
  driver.clk << clk;
  monitor.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 7 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[LD_CTRL_GEOM] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
