// **********************************************************************
// smesh/src/tb_ex_ctrl_row_addr.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026
// Focused ExCtrlRowAddr address-equation test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlRowAddr.hpp"

#include <cstdio>

class RowAddrDriver : public Component {
  DECLARE_COMPONENT(RowAddrDriver);

 public:
  RowAddrDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(smesh::SmeshLocalAddr, a_address_rs1);
  Output(smesh::SmeshLocalAddr, b_address_rs2);
  Output(smesh::SmeshLocalAddr, d_address_rs1);
  Output(u32, a_addr_offset);
  Output(u32, b_fire_counter);
  Output(u32, d_fire_counter);
  Output(u32, block_size);
  Output(bit, ex_read_from_acc);
  Output(bit, start_inputting_a);
  Output(bit, start_inputting_b);
  Output(bit, start_inputting_d);

  void update();
  void reset();
};

class RowAddrMonitor : public Component {
  DECLARE_COMPONENT(RowAddrMonitor);

 public:
  RowAddrMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(smesh::SmeshLocalAddr, a_address);
  Input(smesh::SmeshLocalAddr, b_address);
  Input(smesh::SmeshLocalAddr, d_address);
  Input(u32, dataAbank);
  Input(u32, dataBbank);
  Input(u32, dataDbank);
  Input(u32, dataABankAcc);
  Input(u32, dataBBankAcc);
  Input(u32, dataDBankAcc);
  Input(bit, a_read_from_acc);
  Input(bit, b_read_from_acc);
  Input(bit, d_read_from_acc);
  Input(bit, a_garbage);
  Input(bit, b_garbage);
  Input(bit, d_garbage);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool checked_ = false;
  bool done_ = false;
  bool passed_ = false;
};

RowAddrDriver::RowAddrDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(a_address_rs1,
              b_address_rs2,
              d_address_rs1,
              a_addr_offset,
              b_fire_counter,
              d_fire_counter,
              block_size,
              ex_read_from_acc)
      .writes(start_inputting_a, start_inputting_b, start_inputting_d);
}

void RowAddrDriver::update() {
  a_address_rs1 = smesh::makeSpAddr(4);
  b_address_rs2 = smesh::makeAccAddr(3);
  d_address_rs1 = smesh::makeSpAddr(10);
  a_addr_offset = 2;
  b_fire_counter = 1;
  d_fire_counter = 3;
  block_size = 8;
  ex_read_from_acc = 1;
  start_inputting_a = 1;
  start_inputting_b = 1;
  start_inputting_d = 0;
}

void RowAddrDriver::reset() {
  a_address_rs1.reset(smesh::SmeshLocalAddr{});
  b_address_rs2.reset(smesh::SmeshLocalAddr{});
  d_address_rs1.reset(smesh::SmeshLocalAddr{});
  a_addr_offset.reset(0);
  b_fire_counter.reset(0);
  d_fire_counter.reset(0);
  block_size.reset(0);
  ex_read_from_acc.reset(0);
  start_inputting_a.reset(0);
  start_inputting_b.reset(0);
  start_inputting_d.reset(0);
}

RowAddrMonitor::RowAddrMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(a_address,
             b_address,
             d_address,
             dataAbank,
             dataBbank,
             dataDbank,
             dataABankAcc,
             dataBBankAcc)
      .reads(dataDBankAcc,
             a_read_from_acc,
             b_read_from_acc,
             d_read_from_acc,
             a_garbage,
             b_garbage,
             d_garbage);
}

void RowAddrMonitor::update() {
  if (Sim::state == Sim::SimResetting || checked_) {
    return;
  }

  const auto expected_a = smesh::makeSpAddr(4) + 2;
  const auto expected_b = smesh::makeAccAddr(3) + 1;
  const auto expected_d = smesh::makeSpAddr(10) + 4;

  const bool addrs_ok =
      (*a_address).raw == expected_a.raw &&
      (*b_address).raw == expected_b.raw &&
      (*d_address).raw == expected_d.raw;
  const bool banks_ok =
      dataAbank == expected_a.sp_bank() &&
      dataBbank == expected_b.sp_bank() &&
      dataDbank == expected_d.sp_bank() &&
      dataABankAcc == expected_a.acc_bank() &&
      dataBBankAcc == expected_b.acc_bank() &&
      dataDBankAcc == expected_d.acc_bank();
  const bool read_from_acc_ok =
      a_read_from_acc == 0 &&
      b_read_from_acc != 0 &&
      d_read_from_acc == 0;
  const bool garbage_ok =
      a_garbage == 0 &&
      b_garbage == 0 &&
      d_garbage != 0;

  passed_ = addrs_ok && banks_ok && read_from_acc_ok && garbage_ok;
  checked_ = true;
  done_ = true;
}

void RowAddrMonitor::reset() {
  checked_ = false;
  done_ = false;
  passed_ = false;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  RowAddrDriver driver("Driver");
  smesh::ExCtrlRowAddr row_addr("RowAddr");
  RowAddrMonitor monitor("Monitor");

  row_addr.a_address_rs1 << driver.a_address_rs1;
  row_addr.b_address_rs2 << driver.b_address_rs2;
  row_addr.d_address_rs1 << driver.d_address_rs1;
  row_addr.a_addr_offset << driver.a_addr_offset;
  row_addr.b_fire_counter << driver.b_fire_counter;
  row_addr.d_fire_counter << driver.d_fire_counter;
  row_addr.block_size << driver.block_size;
  row_addr.ex_read_from_acc << driver.ex_read_from_acc;
  row_addr.start_inputting_a << driver.start_inputting_a;
  row_addr.start_inputting_b << driver.start_inputting_b;
  row_addr.start_inputting_d << driver.start_inputting_d;

  monitor.a_address << row_addr.a_address;
  monitor.b_address << row_addr.b_address;
  monitor.d_address << row_addr.d_address;
  monitor.dataAbank << row_addr.dataAbank;
  monitor.dataBbank << row_addr.dataBbank;
  monitor.dataDbank << row_addr.dataDbank;
  monitor.dataABankAcc << row_addr.dataABankAcc;
  monitor.dataBBankAcc << row_addr.dataBBankAcc;
  monitor.dataDBankAcc << row_addr.dataDBankAcc;
  monitor.a_read_from_acc << row_addr.a_read_from_acc;
  monitor.b_read_from_acc << row_addr.b_read_from_acc;
  monitor.d_read_from_acc << row_addr.d_read_from_acc;
  monitor.a_garbage << row_addr.a_garbage;
  monitor.b_garbage << row_addr.b_garbage;
  monitor.d_garbage << row_addr.d_garbage;

  Clock clk;
  driver.clk << clk;
  row_addr.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_ROW_ADDR] %s address_logic\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
