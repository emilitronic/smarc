// **********************************************************************
// smesh/src/tb_ex_ctrl_operand_pack.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026
// Focused ExCtrlOperandPack packaging test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlOperandPack.hpp"

#include <cstdio>

class OperandPackDriver : public Component {
  DECLARE_COMPONENT(OperandPackDriver);

 public:
  OperandPackDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(smesh::SmeshLocalAddr, a_address);
  Output(smesh::SmeshLocalAddr, b_address);
  Output(smesh::SmeshLocalAddr, d_address);
  Output(smesh::SmeshLocalAddr, a_address_rs1);
  Output(smesh::SmeshLocalAddr, b_address_rs2);
  Output(smesh::SmeshLocalAddr, d_address_rs1);
  Output(bit, start_inputting_a);
  Output(bit, start_inputting_b);
  Output(bit, start_inputting_d);
  Output(u32, a_fire_counter);
  Output(u32, b_fire_counter);
  Output(u32, d_fire_counter);
  Output(bit, a_fire_started);
  Output(bit, b_fire_started);
  Output(bit, d_fire_started);

  void update();
  void reset();
};

class OperandPackMonitor : public Component {
  DECLARE_COMPONENT(OperandPackMonitor);

 public:
  OperandPackMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(smesh::ExCtrlOperand, a_operand);
  Input(smesh::ExCtrlOperand, b_operand);
  Input(smesh::ExCtrlOperand, d_operand);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool checked_ = false;
  bool done_ = false;
  bool passed_ = false;
};

namespace {

smesh::SmeshLocalAddr makeGarbageAddr() {
  return smesh::SmeshLocalAddr{
      smesh::kLocalAddrIsAccMask |
      smesh::kLocalAddrAccumulateMask |
      smesh::kLocalAddrReadFullAccRowMask |
      smesh::kLocalAddrGarbageMask |
      smesh::kLocalAddrDataMask};
}

} // namespace

OperandPackDriver::OperandPackDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(a_address,
              b_address,
              d_address,
              a_address_rs1,
              b_address_rs2,
              d_address_rs1,
              start_inputting_a,
              start_inputting_b)
      .writes(start_inputting_d,
              a_fire_counter,
              b_fire_counter,
              d_fire_counter,
              a_fire_started,
              b_fire_started,
              d_fire_started);
}

void OperandPackDriver::update() {
  a_address = smesh::makeSpAddr(8);
  b_address = smesh::makeAccAddr(4);
  d_address = smesh::makeSpAddr(12);
  a_address_rs1 = smesh::makeSpAddr(6);
  b_address_rs2 = makeGarbageAddr();
  d_address_rs1 = smesh::makeSpAddr(10);
  start_inputting_a = 1;
  start_inputting_b = 0;
  start_inputting_d = 1;
  a_fire_counter = 2;
  b_fire_counter = 3;
  d_fire_counter = 4;
  a_fire_started = 1;
  b_fire_started = 0;
  d_fire_started = 1;
}

void OperandPackDriver::reset() {
  a_address.reset(smesh::SmeshLocalAddr{});
  b_address.reset(smesh::SmeshLocalAddr{});
  d_address.reset(smesh::SmeshLocalAddr{});
  a_address_rs1.reset(smesh::SmeshLocalAddr{});
  b_address_rs2.reset(smesh::SmeshLocalAddr{});
  d_address_rs1.reset(smesh::SmeshLocalAddr{});
  start_inputting_a.reset(0);
  start_inputting_b.reset(0);
  start_inputting_d.reset(0);
  a_fire_counter.reset(0);
  b_fire_counter.reset(0);
  d_fire_counter.reset(0);
  a_fire_started.reset(0);
  b_fire_started.reset(0);
  d_fire_started.reset(0);
}

OperandPackMonitor::OperandPackMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(a_operand, b_operand, d_operand);
}

void OperandPackMonitor::update() {
  if (Sim::state == Sim::SimResetting || checked_) {
    return;
  }

  const auto a = *a_operand;
  const auto b = *b_operand;
  const auto d = *d_operand;

  const bool a_ok =
      a.addr.raw == smesh::makeSpAddr(8).raw &&
      a.is_garbage == 0 &&
      a.start_inputting != 0 &&
      a.counter == 2 &&
      a.started != 0 &&
      a.can_be_im2colled != 0 &&
      a.priority == 0;
  const bool b_ok =
      b.addr.raw == smesh::makeAccAddr(4).raw &&
      b.is_garbage != 0 &&
      b.start_inputting == 0 &&
      b.counter == 3 &&
      b.started == 0 &&
      b.can_be_im2colled == 0 &&
      b.priority == 1;
  const bool d_ok =
      d.addr.raw == smesh::makeSpAddr(12).raw &&
      d.is_garbage == 0 &&
      d.start_inputting != 0 &&
      d.counter == 4 &&
      d.started != 0 &&
      d.can_be_im2colled == 0 &&
      d.priority == 2;

  passed_ = a_ok && b_ok && d_ok;
  checked_ = true;
  done_ = true;
}

void OperandPackMonitor::reset() {
  checked_ = false;
  done_ = false;
  passed_ = false;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  OperandPackDriver driver("Driver");
  smesh::ExCtrlOperandPack pack("OperandPack");
  OperandPackMonitor monitor("Monitor");

  pack.a_address << driver.a_address;
  pack.b_address << driver.b_address;
  pack.d_address << driver.d_address;
  pack.a_address_rs1 << driver.a_address_rs1;
  pack.b_address_rs2 << driver.b_address_rs2;
  pack.d_address_rs1 << driver.d_address_rs1;
  pack.start_inputting_a << driver.start_inputting_a;
  pack.start_inputting_b << driver.start_inputting_b;
  pack.start_inputting_d << driver.start_inputting_d;
  pack.a_fire_counter << driver.a_fire_counter;
  pack.b_fire_counter << driver.b_fire_counter;
  pack.d_fire_counter << driver.d_fire_counter;
  pack.a_fire_started << driver.a_fire_started;
  pack.b_fire_started << driver.b_fire_started;
  pack.d_fire_started << driver.d_fire_started;

  monitor.a_operand << pack.a_operand;
  monitor.b_operand << pack.b_operand;
  monitor.d_operand << pack.d_operand;

  Clock clk;
  driver.clk << clk;
  pack.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_OPERAND_PACK] %s packaging\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
