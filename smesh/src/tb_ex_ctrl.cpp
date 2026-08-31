// **********************************************************************
// smesh/src/tb_ex_ctrl.cpp
// **********************************************************************
/*
Focused ExCtrl command/completion handshake test.

cmake --build build --target tb_ex_ctrl -j >/dev/null 2>&1
./build/smesh/tb_ex_ctrl

- Queue/testbench view
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_view
- FSM condition inputs
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_state_view
- Current A/B/D row-address calculation (ExCtrlRowAddr)
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_row_addr_view
- Current A/B/D row-padding calculation (ExCtrlRowPad)
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_row_pad_view
- Execute operand read-request generation (ExCtrlReadReqLogic)
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_read_req_view
- Execute row-feed logic (ExCtrlRowFeedState)
  ./build/smesh/tb_ex_ctrl_row_feed_state -trace '*'/ex_ctrl_row_feed_view
- Completion pending state
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_completion_view
- You can combine them:
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_view';''*'/ex_ctrl_state_view

Use smesh-cascade-testing skill
- $smesh-cascade-testing can invoke it
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrl.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>

TraceKey(ex_ctrl_view); // declare a named TraceKey (and enable it explicitly below)

namespace {

const char* functName(std::uint32_t funct) {
  switch (static_cast<smesh::SmeshFunct>(funct)) {
    case smesh::SmeshFunct::Config:      return "CFG";   // CONFIG
    case smesh::SmeshFunct::Mvin2:       return "M2";    // MVIN2
    case smesh::SmeshFunct::Mvin:        return "MVI";   // MVIN
    case smesh::SmeshFunct::Mvout:       return "MVO";   // MVOUT
    case smesh::SmeshFunct::ComputeFlip: return "CMPF";  // COMPUTE_FLIP
    case smesh::SmeshFunct::ComputeStay: return "CMPS";  // COMPUTE_STAY
    case smesh::SmeshFunct::Preload:     return "PRE";   // PRELOAD
    case smesh::SmeshFunct::Flush:       return "FLU";   // FLUSH
    case smesh::SmeshFunct::Mvin3:       return "M3";    // MVIN3
    case smesh::SmeshFunct::StoreSpad:   return "SSP";   // STORE_SPAD
  }
  return "---";
}

const char* commandName(bool valid, std::uint32_t funct) {
  return valid ? functName(funct) : "---";
}

const char* stateName(std::uint8_t state) {
  switch (static_cast<smesh::ExCtrlFsmState>(state)) {
    case smesh::ExCtrlFsmState::WaitingForCmd: return "WAIT";  // WAITING_FOR_CMD
    case smesh::ExCtrlFsmState::Compute:       return "COMP";  // COMPUTE
    case smesh::ExCtrlFsmState::Flush:         return "FLUS";  // FLUSH
    case smesh::ExCtrlFsmState::Flushing:      return "FLNG";  // FLUSHING
  }
  return "????";
}

} // namespace

class ExCtrlDriver : public Component {
  DECLARE_COMPONENT(ExCtrlDriver);

 public:
  ExCtrlDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoOutput(smesh::SmeshIssue, cmd_out);
  // Test-only taps from ExCtrl; completion is intentionally not checked here.
  Input(u8, control_state);
  Input(bit, config_val);
  Input(bit, config_rs_tag_valid);
  Input(smesh::SmeshRsTag, config_rs_tag);
  InputArray(bit, head_val, smesh::kExCtrlCmdWindow);
  InputArray(smesh::SmeshIssue, head_bits, smesh::kExCtrlCmdWindow);
  Input(smesh::SmeshLocalAddr, rowaddr_a_address);
  Input(smesh::SmeshLocalAddr, rowaddr_b_address);
  Input(smesh::SmeshLocalAddr, rowaddr_d_address);
  Input(u32, rowaddr_a_bank);
  Input(u32, rowaddr_b_bank);
  Input(u32, rowaddr_d_bank);
  Input(bit, rowaddr_a_garbage);
  Input(bit, rowaddr_b_garbage);
  Input(bit, rowaddr_d_garbage);
  Input(bit, rowpad_a_row_not_zero);
  Input(bit, rowpad_b_row_not_zero);
  Input(bit, rowpad_d_row_not_zero);
  Input(u32, rowpad_a_unpadded_cols);
  Input(u32, rowpad_b_unpadded_cols);
  Input(u32, rowpad_d_unpadded_cols);
  OutputArray(bit, spad_read_req_rdy, smesh::kSpBanks);
  InputArray(bit, spad_read_req_val, smesh::kSpBanks);
  InputArray(smesh::SpadBankReadReq, spad_read_req_bits, smesh::kSpBanks);
  OutputArray(bit, accum_read_req_rdy, smesh::kAccBanks);
  InputArray(bit, accum_read_req_val, smesh::kAccBanks);

  void update_memory_ready() {
    for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
      spad_read_req_rdy[bank] = 1;
    }
    for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
      accum_read_req_rdy[bank] = 1;
    }
  }

  void update_issue() {
    if (Sim::state == Sim::SimResetting || next_issue_ >= program_.size() || cmd_out.full()) {
      return;
    }

    cmd_out.push(program_[next_issue_]);
    ++next_issue_;
  }

  void update_completion() {
    if (Sim::state == Sim::SimResetting) {
      return;
    }
    // emit TraceKey with s_trace
    s_trace(ex_ctrl_view,
          "cycle=%d state=%4s cfg{v=%u tv=%u t=%03u} h0{v=%u t=%03u c=%4s} h1{v=%u t=%03u c=%4s} h2{v=%u t=%03u c=%4s}\n",
                cycle_,
                stateName(static_cast<std::uint8_t>(*control_state)),
                static_cast<unsigned>(config_val),
                static_cast<unsigned>(config_rs_tag_valid),
                static_cast<unsigned>(*config_rs_tag),
                static_cast<unsigned>(head_val[0]), static_cast<unsigned>(head_bits[0]->rs_tag), commandName(head_val[0] != 0, head_bits[0]->cmd.funct),
                static_cast<unsigned>(head_val[1]), static_cast<unsigned>(head_bits[1]->rs_tag), commandName(head_val[1] != 0, head_bits[1]->cmd.funct),
                static_cast<unsigned>(head_val[2]), static_cast<unsigned>(head_bits[2]->rs_tag), commandName(head_val[2] != 0, head_bits[2]->cmd.funct));
    for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
      if (head_val[i] == 0) {
        continue;
      }
      for (std::size_t j = 0; j < program_.size(); ++j) {
        if (head_bits[i]->rs_tag == program_[j].rs_tag &&
            head_bits[i]->cmd.funct == program_[j].cmd.funct) {
          seen_[j] = true;
        }
      }
    }
    if (!rowaddr_checked_ &&
        *control_state == static_cast<std::uint8_t>(smesh::ExCtrlFsmState::Compute)) {
      rowaddr_checked_ = true;
      rowaddr_matched_ = rowaddr_a_address->data() == 12 &&
                         rowaddr_b_address->data() == 4 &&
                         rowaddr_d_address->data() == 7 &&
                         *rowaddr_a_bank == smesh::makeSpAddr(12).sp_bank() &&
                         *rowaddr_b_bank == smesh::makeSpAddr(4).sp_bank() &&
                         *rowaddr_d_bank == smesh::makeSpAddr(7).sp_bank() &&
                         rowaddr_a_garbage != 0 &&
                         rowaddr_b_garbage != 0 &&
                         rowaddr_d_garbage == 0;
    }

    if (!rowpad_checked_ &&
        *control_state == static_cast<std::uint8_t>(smesh::ExCtrlFsmState::Compute)) {
      rowpad_checked_ = true;
      rowpad_matched_ = rowpad_a_row_not_zero != 0 &&
                        rowpad_b_row_not_zero != 0 &&
                        rowpad_d_row_not_zero != 0 &&
                        *rowpad_a_unpadded_cols == smesh::kDim &&
                        *rowpad_b_unpadded_cols == smesh::kDim &&
                        *rowpad_d_unpadded_cols == smesh::kDim;
    }

    bool any_read_request = false;
    for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
      any_read_request |= spad_read_req_val[bank] != 0;
    }
    for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
      any_read_request |= accum_read_req_val[bank] != 0;
    }

    if (!read_req_checked_ &&
        *control_state == static_cast<std::uint8_t>(smesh::ExCtrlFsmState::Compute) &&
        any_read_request) {
      read_req_checked_ = true;
      const auto expected = smesh::makeSpAddr(7);
      bool expected_spad_pattern = true;
      for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
        const bool should_be_valid = bank == expected.sp_bank();
        expected_spad_pattern &= (spad_read_req_val[bank] != 0) == should_be_valid;
        if (should_be_valid) {
          expected_spad_pattern &= spad_read_req_bits[bank]->addr == expected.sp_row();
          expected_spad_pattern &= spad_read_req_bits[bank]->from_dma == 0;
        }
      }

      bool no_accum_request = true;
      for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
        no_accum_request &= accum_read_req_val[bank] == 0;
      }
      read_req_matched_ = expected_spad_pattern && no_accum_request;
    }

    matched_ = seen_[0] && seen_[1] && seen_[2] &&
               rowaddr_checked_ && rowaddr_matched_ &&
               rowpad_checked_ && rowpad_matched_ &&
               read_req_checked_ && read_req_matched_;
    done_ = matched_;
    ++cycle_;
  }

  void reset() {
    next_issue_ = 0;
    seen_ = {};
    rowaddr_checked_ = false;
    rowaddr_matched_ = false;
    rowpad_checked_ = false;
    rowpad_matched_ = false;
    read_req_checked_ = false;
    read_req_matched_ = false;
    done_ = false;
    matched_ = false;
    cycle_ = 0;
  }

  bool done() const { return done_; }
  bool matched() const { return matched_; }

 private:
  static smesh::SmeshIssue makeIssue(smesh::SmeshRsTag tag, smesh::SmeshFunct funct,
                                     std::uint64_t rs1 = 0, std::uint64_t rs2 = 0,
                                     bool tag_valid = true) {
    smesh::SmeshIssue issue{};
    issue.rs_tag_valid = bit(tag_valid);
    issue.rs_tag = tag;
    issue.cmd.funct = static_cast<std::uint32_t>(funct);
    issue.cmd.rs1 = rs1;
    issue.cmd.rs2 = rs2;
    return issue;
  }

  // Use distinct 4x4 local operands so the decoder and first-row address
  // calculations can be checked directly in the next test step.
  const std::array<smesh::SmeshIssue, 3> program_{
      makeIssue(7, smesh::SmeshFunct::Config,
                smesh::packConfigExecuteRs1(1),
                smesh::packConfigExecuteRs2(1)),
      makeIssue(8, smesh::SmeshFunct::Preload,
                smesh::packLocal(smesh::makeSpAddr(4), {smesh::kDim, smesh::kDim}),
                smesh::packLocal(smesh::makeAccAddr(8), {smesh::kDim, smesh::kDim}),
                true),
      makeIssue(9, smesh::SmeshFunct::ComputeStay,
                smesh::packLocal(smesh::makeSpAddr(12), {smesh::kDim, smesh::kDim}),
                smesh::packLocal(smesh::makeSpAddr(4), {smesh::kDim, smesh::kDim})),
  };

  int cycle_ = 0;
  std::size_t next_issue_ = 0;
  std::array<bool, 3> seen_{};
  bool rowaddr_checked_ = false;
  bool rowaddr_matched_ = false;
  bool rowpad_checked_ = false;
  bool rowpad_matched_ = false;
  bool read_req_checked_ = false;
  bool read_req_matched_ = false;
  bool done_ = false;
  bool matched_ = false;
};

ExCtrlDriver::ExCtrlDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update_issue).writes(cmd_out);
  UPDATE(update_memory_ready).writes(spad_read_req_rdy, accum_read_req_rdy);
  UPDATE(update_completion).reads(control_state, config_val, config_rs_tag_valid, config_rs_tag,
                                  head_val, head_bits)
                           .reads(rowaddr_a_address, rowaddr_b_address, rowaddr_d_address,
                                  rowaddr_a_bank, rowaddr_b_bank, rowaddr_d_bank)
                           .reads(rowaddr_a_garbage, rowaddr_b_garbage, rowaddr_d_garbage)
                           .reads(rowpad_a_row_not_zero, rowpad_b_row_not_zero, rowpad_d_row_not_zero,
                                  rowpad_a_unpadded_cols, rowpad_b_unpadded_cols,
                                  rowpad_d_unpadded_cols)
                           .reads(spad_read_req_val, spad_read_req_bits,
                                  accum_read_req_val);
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::ExCtrl ctrl("ExCtrl");
  ExCtrlDriver driver("Driver");

  ctrl.cmd_in << driver.cmd_out;
  driver.control_state << ctrl.control_state;
  driver.config_val << ctrl.config_val;
  driver.config_rs_tag_valid << ctrl.config_rs_tag_valid;
  driver.config_rs_tag << ctrl.config_rs_tag;
  driver.rowaddr_a_address << ctrl.rowaddr_a_address;
  driver.rowaddr_b_address << ctrl.rowaddr_b_address;
  driver.rowaddr_d_address << ctrl.rowaddr_d_address;
  driver.rowaddr_a_bank << ctrl.rowaddr_a_bank;
  driver.rowaddr_b_bank << ctrl.rowaddr_b_bank;
  driver.rowaddr_d_bank << ctrl.rowaddr_d_bank;
  driver.rowaddr_a_garbage << ctrl.rowaddr_a_garbage;
  driver.rowaddr_b_garbage << ctrl.rowaddr_b_garbage;
  driver.rowaddr_d_garbage << ctrl.rowaddr_d_garbage;
  driver.rowpad_a_row_not_zero << ctrl.rowpad_a_row_not_zero;
  driver.rowpad_b_row_not_zero << ctrl.rowpad_b_row_not_zero;
  driver.rowpad_d_row_not_zero << ctrl.rowpad_d_row_not_zero;
  driver.rowpad_a_unpadded_cols << ctrl.rowpad_a_unpadded_cols;
  driver.rowpad_b_unpadded_cols << ctrl.rowpad_b_unpadded_cols;
  driver.rowpad_d_unpadded_cols << ctrl.rowpad_d_unpadded_cols;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    ctrl.spad_read_req_rdy[bank] << driver.spad_read_req_rdy[bank];
    driver.spad_read_req_val[bank] << ctrl.spad_read_req_val[bank];
    driver.spad_read_req_bits[bank] << ctrl.spad_read_req_bits[bank];
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    ctrl.accum_read_req_rdy[bank] << driver.accum_read_req_rdy[bank];
    driver.accum_read_req_val[bank] << ctrl.accum_read_req_val[bank];
  }
  for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
    driver.head_val[i] << ctrl.cmd_queue_head_val[i];
    driver.head_bits[i] << ctrl.cmd_queue_head_bits[i];
  }
  ctrl.cmd_in.setDelay(1);

  Clock clk;
  ctrl.clk << clk;
  driver.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 8; ++i) {
    Sim::run();
  }

  const bool ok = driver.done() && driver.matched();
  std::printf("[EX_CTRL] %s config_preload_compute_first_row_addr_pad_read_req\n",
              ok ? "PASS" : "FAIL");
  descore::flushLog(); // flush log before exiting because trace o/p is buffered
  return ok ? 0 : 1;
}
