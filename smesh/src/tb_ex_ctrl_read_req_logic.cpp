// **********************************************************************
// smesh/src/tb_ex_ctrl_read_req_logic.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026
// Focused ExCtrlReadReqLogic skeleton test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlReadReqLogic.hpp"

#include <cstdio>

class ReadReqLogicDriver : public Component {
  DECLARE_COMPONENT(ReadReqLogicDriver);

 public:
  ReadReqLogicDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, start_inputting_a);
  Output(bit, start_inputting_b);
  Output(bit, start_inputting_d);
  Output(smesh::SmeshLocalAddr, a_address);
  Output(smesh::SmeshLocalAddr, b_address);
  Output(smesh::SmeshLocalAddr, d_address);
  Output(bit, a_valid);
  Output(bit, b_valid);
  Output(bit, d_valid);
  Output(bit, a_row_is_not_all_zeros);
  Output(bit, b_row_is_not_all_zeros);
  Output(bit, d_row_is_not_all_zeros);
  Output(bit, multiply_garbage);
  Output(bit, accumulate_zeros);
  Output(bit, preload_zeros);
  Output(bit, a_read_from_acc);
  Output(bit, b_read_from_acc);
  Output(bit, d_read_from_acc);
  Output(u32, dataAbank);
  Output(u32, dataBbank);
  Output(u32, dataDbank);
  Output(u32, dataABankAcc);
  Output(u32, dataBBankAcc);
  Output(u32, dataDBankAcc);
  OutputArray(bit, spad_read_req_rdy, smesh::kSpBanks);
  OutputArray(bit, accum_read_req_rdy, smesh::kAccBanks);
  Output(bit, cntl_rdy);
  Output(u32, acc_scale);
  Output(u8, activation);

  void update();
  void reset();
};

class ReadReqLogicMonitor : public Component {
  DECLARE_COMPONENT(ReadReqLogicMonitor);

 public:
  ReadReqLogicMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, a_ready);
  Input(bit, b_ready);
  Input(bit, d_ready);
  InputArray(bit, spad_read_req_val, smesh::kSpBanks);
  InputArray(bit, accum_read_req_val, smesh::kAccBanks);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool checked_ = false;
  bool done_ = false;
  bool passed_ = false;
};

ReadReqLogicDriver::ReadReqLogicDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(start_inputting_a,
              start_inputting_b,
              start_inputting_d,
              a_address,
              b_address,
              d_address,
              a_valid,
              b_valid)
      .writes(d_valid,
              a_row_is_not_all_zeros,
              b_row_is_not_all_zeros,
              d_row_is_not_all_zeros,
              multiply_garbage,
              accumulate_zeros,
              preload_zeros,
              a_read_from_acc)
      .writes(b_read_from_acc,
              d_read_from_acc,
              dataAbank,
              dataBbank,
              dataDbank,
              dataABankAcc,
              dataBBankAcc,
              dataDBankAcc)
      .writes(spad_read_req_rdy,
              accum_read_req_rdy,
              cntl_rdy,
              acc_scale,
              activation);
}

void ReadReqLogicDriver::update() {
  start_inputting_a = 1;
  start_inputting_b = 1;
  start_inputting_d = 1;
  a_address = smesh::makeSpAddr(0);
  b_address = smesh::makeSpAddr(1);
  d_address = smesh::makeAccAddr(0);
  a_valid = 1;
  b_valid = 1;
  d_valid = 1;
  a_row_is_not_all_zeros = 1;
  b_row_is_not_all_zeros = 1;
  d_row_is_not_all_zeros = 1;
  multiply_garbage = 0;
  accumulate_zeros = 0;
  preload_zeros = 0;
  a_read_from_acc = 0;
  b_read_from_acc = 0;
  d_read_from_acc = 1;
  dataAbank = 0;
  dataBbank = 1;
  dataDbank = 0;
  dataABankAcc = 0;
  dataBBankAcc = 0;
  dataDBankAcc = 0;
  cntl_rdy = 1;
  acc_scale = 0;
  activation = 0;

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    spad_read_req_rdy[bank] = 1;
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    accum_read_req_rdy[bank] = 1;
  }
}

void ReadReqLogicDriver::reset() {
  start_inputting_a.reset(0);
  start_inputting_b.reset(0);
  start_inputting_d.reset(0);
  a_address.reset(smesh::SmeshLocalAddr{});
  b_address.reset(smesh::SmeshLocalAddr{});
  d_address.reset(smesh::SmeshLocalAddr{});
  a_valid.reset(0);
  b_valid.reset(0);
  d_valid.reset(0);
  a_row_is_not_all_zeros.reset(0);
  b_row_is_not_all_zeros.reset(0);
  d_row_is_not_all_zeros.reset(0);
  multiply_garbage.reset(0);
  accumulate_zeros.reset(0);
  preload_zeros.reset(0);
  a_read_from_acc.reset(0);
  b_read_from_acc.reset(0);
  d_read_from_acc.reset(0);
  dataAbank.reset(0);
  dataBbank.reset(0);
  dataDbank.reset(0);
  dataABankAcc.reset(0);
  dataBBankAcc.reset(0);
  dataDBankAcc.reset(0);
  cntl_rdy.reset(0);
  acc_scale.reset(0);
  activation.reset(0);

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    spad_read_req_rdy[bank].reset(0);
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    accum_read_req_rdy[bank].reset(0);
  }
}

ReadReqLogicMonitor::ReadReqLogicMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(a_ready, b_ready, d_ready, spad_read_req_val, accum_read_req_val);
}

void ReadReqLogicMonitor::update() {
  if (Sim::state == Sim::SimResetting || checked_) {
    return;
  }

  bool no_spad_req = true;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    no_spad_req = no_spad_req && spad_read_req_val[bank] == 0;
  }

  bool no_accum_req = true;
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    no_accum_req = no_accum_req && accum_read_req_val[bank] == 0;
  }

  passed_ = a_ready == 0 && b_ready == 0 && d_ready == 0 && no_spad_req && no_accum_req;
  checked_ = true;
  done_ = true;
}

void ReadReqLogicMonitor::reset() {
  checked_ = false;
  done_ = false;
  passed_ = false;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  ReadReqLogicDriver driver("Driver");
  smesh::ExCtrlReadReqLogic logic("ReadReqLogic");
  ReadReqLogicMonitor monitor("Monitor");

  logic.start_inputting_a << driver.start_inputting_a;
  logic.start_inputting_b << driver.start_inputting_b;
  logic.start_inputting_d << driver.start_inputting_d;
  logic.a_address << driver.a_address;
  logic.b_address << driver.b_address;
  logic.d_address << driver.d_address;
  logic.a_valid << driver.a_valid;
  logic.b_valid << driver.b_valid;
  logic.d_valid << driver.d_valid;
  logic.a_row_is_not_all_zeros << driver.a_row_is_not_all_zeros;
  logic.b_row_is_not_all_zeros << driver.b_row_is_not_all_zeros;
  logic.d_row_is_not_all_zeros << driver.d_row_is_not_all_zeros;
  logic.multiply_garbage << driver.multiply_garbage;
  logic.accumulate_zeros << driver.accumulate_zeros;
  logic.preload_zeros << driver.preload_zeros;
  logic.a_read_from_acc << driver.a_read_from_acc;
  logic.b_read_from_acc << driver.b_read_from_acc;
  logic.d_read_from_acc << driver.d_read_from_acc;
  logic.dataAbank << driver.dataAbank;
  logic.dataBbank << driver.dataBbank;
  logic.dataDbank << driver.dataDbank;
  logic.dataABankAcc << driver.dataABankAcc;
  logic.dataBBankAcc << driver.dataBBankAcc;
  logic.dataDBankAcc << driver.dataDBankAcc;
  logic.cntl_rdy << driver.cntl_rdy;
  logic.acc_scale << driver.acc_scale;
  logic.activation << driver.activation;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    logic.spad_read_req_rdy[bank] << driver.spad_read_req_rdy[bank];
    monitor.spad_read_req_val[bank] << logic.spad_read_req_val[bank];
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    logic.accum_read_req_rdy[bank] << driver.accum_read_req_rdy[bank];
    monitor.accum_read_req_val[bank] << logic.accum_read_req_val[bank];
  }

  monitor.a_ready << logic.a_ready;
  monitor.b_ready << logic.b_ready;
  monitor.d_ready << logic.d_ready;

  Clock clk;
  driver.clk << clk;
  logic.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_READ_REQ_LOGIC] %s skeleton\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
