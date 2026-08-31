// **********************************************************************
// smesh/src/tb_ex_ctrl_read_req_logic.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026
// Focused ExCtrlReadReqLogic request generation and backpressure test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlReadReqLogic.hpp"

#include <array>
#include <cstdio>

namespace {

constexpr int kCaseCount = 8;

struct ReadReqCase {
  const char* name = "";

  bit start_a = 1;
  bit start_b = 1;
  bit start_d = 1;
  smesh::SmeshLocalAddr a_addr = smesh::makeSpAddr(3);
  smesh::SmeshLocalAddr b_addr = smesh::makeSpAddr(smesh::kSpBankRows + 1);
  smesh::SmeshLocalAddr d_addr = smesh::makeAccAddr(smesh::kAccBankRows + 2);
  bit a_valid = 1;
  bit b_valid = 1;
  bit d_valid = 1;
  bit a_row_real = 1;
  bit b_row_real = 1;
  bit d_row_real = 1;
  bit multiply_garbage = 0;
  bit accumulate_zeros = 0;
  bit preload_zeros = 0;
  bit a_from_acc = 0;
  bit b_from_acc = 0;
  bit d_from_acc = 1;
  u32 a_sp_bank = 0;
  u32 b_sp_bank = 1;
  u32 d_sp_bank = 0;
  u32 a_acc_bank = 0;
  u32 b_acc_bank = 0;
  u32 d_acc_bank = 1;
  std::array<bit, smesh::kSpBanks> spad_rdy{};
  std::array<bit, smesh::kAccBanks> accum_rdy{};
  bit cntl_rdy = 1;
  u32 acc_scale = 0x12345678u;
  u8 activation = 2;
  bit im2col_wire = 0;
  bit im2col_en = 0;

  bit expected_a_ready = 1;
  bit expected_b_ready = 1;
  bit expected_d_ready = 1;
  std::array<bit, smesh::kSpBanks> expected_spad_val{};
  std::array<u32, smesh::kSpBanks> expected_spad_addr{};
  std::array<bit, smesh::kAccBanks> expected_accum_val{};
  std::array<u32, smesh::kAccBanks> expected_accum_addr{};
};

void clearExpectedRequests(ReadReqCase& test) {
  test.expected_spad_val.fill(0);
  test.expected_spad_addr.fill(0);
  test.expected_accum_val.fill(0);
  test.expected_accum_addr.fill(0);
}

ReadReqCase readReqCase(int index) {
  ReadReqCase test{};
  test.spad_rdy.fill(1);
  test.accum_rdy.fill(1);
  test.expected_spad_val[0] = 1;
  test.expected_spad_addr[0] = 3;
  test.expected_spad_val[1] = 1;
  test.expected_spad_addr[1] = 1;
  test.expected_accum_val[1] = 1;
  test.expected_accum_addr[1] = 2;

  switch (index) {
    case 0:
      test.name = "mixed A/B/D reads";
      break;

    case 1:
      test.name = "spad backpressure";
      test.start_b = 0;
      test.start_d = 0;
      test.spad_rdy[0] = 0;
      clearExpectedRequests(test);
      test.expected_spad_val[0] = 1;
      test.expected_spad_addr[0] = 3;
      test.expected_a_ready = 0;
      break;

    case 2:
      test.name = "accum backpressure";
      test.start_a = 0;
      test.start_b = 0;
      test.accum_rdy[1] = 0;
      clearExpectedRequests(test);
      test.expected_accum_val[1] = 1;
      test.expected_accum_addr[1] = 2;
      test.expected_d_ready = 0;
      break;

    case 3:
      test.name = "control queue blocked";
      test.start_b = 0;
      test.start_d = 0;
      test.cntl_rdy = 0;
      clearExpectedRequests(test);
      break;

    case 4:
      test.name = "garbage zero flags";
      test.multiply_garbage = 1;
      test.accumulate_zeros = 1;
      test.preload_zeros = 1;
      clearExpectedRequests(test);
      break;

    case 5:
      test.name = "padded rows";
      test.a_row_real = 0;
      test.b_row_real = 0;
      test.d_row_real = 0;
      clearExpectedRequests(test);
      break;

    case 6:
      test.name = "A uses im2col";
      test.start_b = 0;
      test.start_d = 0;
      test.im2col_wire = 1;
      test.im2col_en = 1;
      clearExpectedRequests(test);
      break;

    case 7:
      test.name = "A accumulator read";
      test.start_b = 0;
      test.start_d = 0;
      test.a_addr = smesh::makeAccAddr(3);
      test.a_from_acc = 1;
      test.a_acc_bank = 0;
      clearExpectedRequests(test);
      test.expected_accum_val[0] = 1;
      test.expected_accum_addr[0] = 3;
      break;

    default:
      test.name = "invalid case";
      clearExpectedRequests(test);
      test.expected_a_ready = 0;
      test.expected_b_ready = 0;
      test.expected_d_ready = 0;
      break;
  }

  return test;
}

TraceKey(read_req_view);

} // namespace

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
  Output(bit, im2col_wire);
  Output(bit, im2col_en);

  void update();
  void reset();

 private:
  int cycle_ = 0;
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
  InputArray(u32, spad_read_req_addr, smesh::kSpBanks);
  InputArray(bit, spad_read_req_from_dma, smesh::kSpBanks);
  InputArray(bit, accum_read_req_val, smesh::kAccBanks);
  InputArray(u32, accum_read_req_addr, smesh::kAccBanks);
  InputArray(u32, accum_read_req_scale, smesh::kAccBanks);
  InputArray(bit, accum_read_req_full, smesh::kAccBanks);
  InputArray(u8, accum_read_req_act, smesh::kAccBanks);
  InputArray(u32, accum_read_req_igelu_qb, smesh::kAccBanks);
  InputArray(u32, accum_read_req_igelu_qc, smesh::kAccBanks);
  InputArray(u32, accum_read_req_iexp_qln2, smesh::kAccBanks);
  InputArray(u32, accum_read_req_iexp_qln2_inv, smesh::kAccBanks);
  InputArray(bit, accum_read_req_from_dma, smesh::kAccBanks);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  int cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
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
              activation,
              im2col_wire,
              im2col_en);
}

void ReadReqLogicDriver::update() {
  const auto test = readReqCase(cycle_);

  start_inputting_a = test.start_a;
  start_inputting_b = test.start_b;
  start_inputting_d = test.start_d;
  a_address = test.a_addr;
  b_address = test.b_addr;
  d_address = test.d_addr;
  a_valid = test.a_valid;
  b_valid = test.b_valid;
  d_valid = test.d_valid;
  a_row_is_not_all_zeros = test.a_row_real;
  b_row_is_not_all_zeros = test.b_row_real;
  d_row_is_not_all_zeros = test.d_row_real;
  multiply_garbage = test.multiply_garbage;
  accumulate_zeros = test.accumulate_zeros;
  preload_zeros = test.preload_zeros;
  a_read_from_acc = test.a_from_acc;
  b_read_from_acc = test.b_from_acc;
  d_read_from_acc = test.d_from_acc;
  dataAbank = test.a_sp_bank;
  dataBbank = test.b_sp_bank;
  dataDbank = test.d_sp_bank;
  dataABankAcc = test.a_acc_bank;
  dataBBankAcc = test.b_acc_bank;
  dataDBankAcc = test.d_acc_bank;
  cntl_rdy = test.cntl_rdy;
  acc_scale = test.acc_scale;
  activation = test.activation;
  im2col_wire = test.im2col_wire;
  im2col_en = test.im2col_en;

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    spad_read_req_rdy[bank] = test.spad_rdy[bank];
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    accum_read_req_rdy[bank] = test.accum_rdy[bank];
  }

  ++cycle_;
}

void ReadReqLogicDriver::reset() {
  cycle_ = 0;
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
  im2col_wire.reset(0);
  im2col_en.reset(0);

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    spad_read_req_rdy[bank].reset(0);
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    accum_read_req_rdy[bank].reset(0);
  }
}

ReadReqLogicMonitor::ReadReqLogicMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(a_ready,
             b_ready,
             d_ready,
             spad_read_req_val,
             spad_read_req_addr,
             spad_read_req_from_dma)
      .reads(accum_read_req_val,
             accum_read_req_addr,
             accum_read_req_scale,
             accum_read_req_full,
             accum_read_req_act)
      .reads(accum_read_req_igelu_qb,
             accum_read_req_igelu_qc,
             accum_read_req_iexp_qln2,
             accum_read_req_iexp_qln2_inv,
             accum_read_req_from_dma);
}

void ReadReqLogicMonitor::update() {
  if (Sim::state == Sim::SimResetting || done_) {
    return;
  }

  const auto test = readReqCase(cycle_);
  bool case_passed =
      a_ready == test.expected_a_ready &&
      b_ready == test.expected_b_ready &&
      d_ready == test.expected_d_ready;
  unsigned spad_mask = 0;
  unsigned expected_spad_mask = 0;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    const bool valid = spad_read_req_val[bank] != 0;
    const bool expected_valid = test.expected_spad_val[bank] != 0;
    spad_mask |= static_cast<unsigned>(valid) << bank;
    expected_spad_mask |= static_cast<unsigned>(expected_valid) << bank;
    case_passed = case_passed && valid == expected_valid;
    case_passed = case_passed && spad_read_req_from_dma[bank] == 0;
    if (expected_valid) {
      case_passed = case_passed && spad_read_req_addr[bank] == test.expected_spad_addr[bank];
    }
  }

  unsigned accum_mask = 0;
  unsigned expected_accum_mask = 0;
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    const bool valid = accum_read_req_val[bank] != 0;
    const bool expected_valid = test.expected_accum_val[bank] != 0;
    accum_mask |= static_cast<unsigned>(valid) << bank;
    expected_accum_mask |= static_cast<unsigned>(expected_valid) << bank;
    case_passed = case_passed && valid == expected_valid;
    case_passed = case_passed && accum_read_req_from_dma[bank] == 0;
    case_passed = case_passed && accum_read_req_scale[bank] == test.acc_scale;
    case_passed = case_passed && accum_read_req_act[bank] == test.activation;
    case_passed = case_passed && accum_read_req_full[bank] == 0;
    case_passed = case_passed && accum_read_req_igelu_qb[bank] == 0;
    case_passed = case_passed && accum_read_req_igelu_qc[bank] == 0;
    case_passed = case_passed && accum_read_req_iexp_qln2[bank] == 0;
    case_passed = case_passed && accum_read_req_iexp_qln2_inv[bank] == 0;
    if (expected_valid) {
      case_passed = case_passed && accum_read_req_addr[bank] == test.expected_accum_addr[bank];
    }
  }

  s_trace(read_req_view,
          "case=%02d %-21s rdy{%u%u%u} sp{0x%x} acc{0x%x} expected rdy{%u%u%u} sp{0x%x} acc{0x%x}\n",
          cycle_,
          test.name,
          static_cast<unsigned>(a_ready != 0),
          static_cast<unsigned>(b_ready != 0),
          static_cast<unsigned>(d_ready != 0),
          spad_mask,
          accum_mask,
          static_cast<unsigned>(test.expected_a_ready != 0),
          static_cast<unsigned>(test.expected_b_ready != 0),
          static_cast<unsigned>(test.expected_d_ready != 0),
          expected_spad_mask,
          expected_accum_mask);

  if (!case_passed) {
    std::printf("[EX_CTRL_READ_REQ_LOGIC] case %d (%s) mismatch\n", cycle_, test.name);
    for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
      if (test.expected_spad_val[bank] != 0) {
        std::printf("  spad[%zu] addr=%u expected=%u from_dma=%u\n",
                    bank,
                    static_cast<unsigned>(*spad_read_req_addr[bank]),
                    static_cast<unsigned>(test.expected_spad_addr[bank]),
                    static_cast<unsigned>(spad_read_req_from_dma[bank] != 0));
      }
    }
    for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
      if (test.expected_accum_val[bank] != 0) {
        std::printf("  accum[%zu] addr=%u expected=%u scale=0x%x act=%u full=%u from_dma=%u\n",
                    bank,
                    static_cast<unsigned>(*accum_read_req_addr[bank]),
                    static_cast<unsigned>(test.expected_accum_addr[bank]),
                    static_cast<unsigned>(*accum_read_req_scale[bank]),
                    static_cast<unsigned>(*accum_read_req_act[bank]),
                    static_cast<unsigned>(accum_read_req_full[bank] != 0),
                    static_cast<unsigned>(accum_read_req_from_dma[bank] != 0));
      }
    }
    passed_ = false;
  }

  ++cycle_;
  done_ = cycle_ == kCaseCount;
}

void ReadReqLogicMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = true;
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
  logic.im2col_wire << driver.im2col_wire;
  logic.im2col_en << driver.im2col_en;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    logic.spad_read_req_rdy[bank] << driver.spad_read_req_rdy[bank];
    monitor.spad_read_req_val[bank] << logic.spad_read_req_val[bank];
    monitor.spad_read_req_addr[bank] << logic.spad_read_req_addr[bank];
    monitor.spad_read_req_from_dma[bank] << logic.spad_read_req_from_dma[bank];
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    logic.accum_read_req_rdy[bank] << driver.accum_read_req_rdy[bank];
    monitor.accum_read_req_val[bank] << logic.accum_read_req_val[bank];
    monitor.accum_read_req_addr[bank] << logic.accum_read_req_addr[bank];
    monitor.accum_read_req_scale[bank] << logic.accum_read_req_scale[bank];
    monitor.accum_read_req_full[bank] << logic.accum_read_req_full[bank];
    monitor.accum_read_req_act[bank] << logic.accum_read_req_act[bank];
    monitor.accum_read_req_igelu_qb[bank] << logic.accum_read_req_igelu_qb[bank];
    monitor.accum_read_req_igelu_qc[bank] << logic.accum_read_req_igelu_qc[bank];
    monitor.accum_read_req_iexp_qln2[bank] << logic.accum_read_req_iexp_qln2[bank];
    monitor.accum_read_req_iexp_qln2_inv[bank] << logic.accum_read_req_iexp_qln2_inv[bank];
    monitor.accum_read_req_from_dma[bank] << logic.accum_read_req_from_dma[bank];
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
  for (int i = 0; i < kCaseCount + 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_READ_REQ_LOGIC] %s request_generation\n", ok ? "PASS" : "FAIL");
  descore::flushLog();
  return ok ? 0 : 1;
}
