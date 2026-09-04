// **********************************************************************
// smesh/src/tb_ex_ctrl.cpp
// **********************************************************************
/*
Focused ExCtrl command/completion handshake test.

cmake --build build --target tb_ex_ctrl -j >/dev/null 2>&1
./build/smesh/tb_ex_ctrl

- Queue/testbench view
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_view

- ExCtrlState: FSM condition inputs
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_state_view

- ExCtrlRowAddr: Current A/B/D row-address calculation
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_row_addr_view

- ExCtrlRowPad:Current A/B/D row-padding calculation
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_row_pad_view

- ExCtrlReadReqLogic: Execute operand read-request generation 
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_read_req_view

- Test SPAD model: accepted requests and fixed-latency responses
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_spad_mem_view

- ExCtrlRowFeedState: Execute row-feed logic
./build/smesh/tb_ex_ctrl_row_feed_state -trace '*'/row_feed_

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
#include "tb_ex_ctrl_scenarios.hpp" // ExCtrl test scenarios

#include <array>
#include <cassert>
#include <cstdio>

TraceKey(ex_ctrl_view); // declare a named TraceKey (and enable it explicitly below)
TraceKey(ex_ctrl_spad_mem_view);

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

const auto kScenario = smesh::tb::makeConfigPreloadComputeScenario();

// ************************
// ********* SPAD *********
// Elastic fixed-latency SPAD pipeline model used only by this testbench.

// Contents of test SPAD's request pipelines; valid[bank][stage], bits[bank][stage]
// 0 -> 1 -> 2 -> 3
struct ExCtrlSpadPipelineState {
  std::array<std::array<bit,                 smesh::kSpadReadDelay>, smesh::kSpBanks> valid{};
  std::array<std::array<smesh::SpadReadResp, smesh::kSpadReadDelay>, smesh::kSpBanks> bits{};
};
// SPAD component declaration
class ExCtrlSpadModel : public Component {
  DECLARE_COMPONENT(ExCtrlSpadModel, SpadModel);

 public:
  ExCtrlSpadModel(std::string name, COMPONENT_CTOR);

  Clock(clk);
  InputArray(bit,                    req_val,  smesh::kSpBanks); // ExC puts valid req
  OutputArray(bit,                   req_rdy,  smesh::kSpBanks);
  InputArray(smesh::SpadBankReadReq, req_bits, smesh::kSpBanks); // ExC puts local bank-local row addr

  OutputArray(bit,                 resp_val,  smesh::kSpBanks);  // SPAD puts valid resp
  InputArray(bit,                  resp_rdy,  smesh::kSpBanks);
  OutputArray(smesh::SpadReadResp, resp_bits, smesh::kSpBanks);  // SPAD puts row and metadata

  void updateRespView();
  void updateReady();
  void updateNextState();
  void reset();

 private:
  // Activation state
  Output(bit,                       active_);     // expose active register value across an explicit cycle boundary
  Register(bit,                     active_reg_); // zeroed at reset, set to 1 afterwards, prevents transactions in reset
  // Pipeline state
  Output(ExCtrlSpadPipelineState,   pipeline_state_); // pipelne contents visible during current cycle
  Register(ExCtrlSpadPipelineState, pipeline_);       // pipeline values being prep'd for next cycle
};
// SPAD component constructor and its three update functions
ExCtrlSpadModel::ExCtrlSpadModel(std::string /*name*/, IMPL_CTOR) {
  static_assert(smesh::kSpadReadDelay > 0);
  // Expose the committed register value across an explicit cycle boundary.
  active_ <= active_reg_;
  pipeline_state_ <= pipeline_;
  UPDATE(updateRespView)
      .reads(pipeline_state_)
      .writes(resp_val, resp_bits);
  UPDATE(updateReady)
      .reads(active_, resp_rdy, pipeline_state_)
      .writes(req_rdy);
  UPDATE(updateNextState)
      .reads(active_, req_val, req_rdy, req_bits, resp_rdy, pipeline_state_)
      .writes(active_reg_, pipeline_);
}
// reads current final pipleline stages and presents them through resp_val and resp_bits
void ExCtrlSpadModel::updateRespView() {
  const auto current = *pipeline_state_;       // snapshot of current pipeline state
  const auto last = smesh::kSpadReadDelay - 1; // last stage of the pipeline
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    resp_val[bank]  = current.valid[bank][last]; // final stage feeds resp port
    resp_bits[bank] = current.bits[bank][last];  // final stage feeds resp port
    if (current.valid[bank][last] != 0) {
      const auto& response = current.bits[bank][last];
      trace(ex_ctrl_spad_mem_view,
            "offer bank=%u row=%u data={%d,%d,%d,%d}\n",
            static_cast<unsigned>(bank),
            static_cast<unsigned>(response.laddr.full_sp_addr()),
            static_cast<int>(response.data[0]), static_cast<int>(response.data[1]),
            static_cast<int>(response.data[2]), static_cast<int>(response.data[3]));
    }
  }
}
// determines whether each pipeline can accept another req and drives req_rdy
void ExCtrlSpadModel::updateReady() {
  const auto current = *pipeline_state_;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    std::array<bool, smesh::kSpadReadDelay> stage_rdy{};
    const auto last = smesh::kSpadReadDelay - 1;
    // final stg rdy when it's empty or ExCtrl is ready to consume its reponse
    stage_rdy[last] = current.valid[bank][last] == 0 || resp_rdy[bank] != 0; 
    for (std::size_t stage = last; stage-- > 0;) {
      // earlier stg rdy when it's empdy or following stg is ready
      stage_rdy[stage] = current.valid[bank][stage] == 0 || stage_rdy[stage + 1];
    }
    // SPAD can accept a new request if the first stage is ready
    req_rdy[bank] = bit(active_ != 0 && stage_rdy[0]);
  }
}
// calculate what should be stored in pipeline regs for next cycle
void ExCtrlSpadModel::updateNextState() {
  active_reg_ = 1;

  // Reset-time port values are not transactions; keep the pipeline empty.
  if (Sim::state == Sim::SimResetting || active_ == 0) {
    pipeline_ = ExCtrlSpadPipelineState{};
    return;
  }

  const auto current = *pipeline_state_;
  auto next = current;

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    std::array<bool, smesh::kSpadReadDelay> stage_rdy{};
    const auto last = smesh::kSpadReadDelay - 1;
    stage_rdy[last] = current.valid[bank][last] == 0 || resp_rdy[bank] != 0;
    for (std::size_t stage = last; stage-- > 0;) {
      stage_rdy[stage] = current.valid[bank][stage] == 0 || stage_rdy[stage + 1];
    }

    // Each register stage advances only when the following stage can accept it.
    for (std::size_t stage = smesh::kSpadReadDelay; stage-- > 1;) {
      if (stage_rdy[stage]) {
        next.valid[bank][stage] = current.valid[bank][stage - 1];
        next.bits[bank][stage]  = current.bits[bank][stage - 1];
      }
    }

    if (stage_rdy[0]) {
      const bool request_fire = req_val[bank] != 0 && req_rdy[bank] != 0;
      next.valid[bank][0] = bit(request_fire);
      if (request_fire) {
        const auto local_row = static_cast<std::uint32_t>(req_bits[bank]->addr);
        assert_always(local_row < smesh::kSpBankRows,
                      "SPAD bank %u received out-of-range local row %u",
                      static_cast<unsigned>(bank), static_cast<unsigned>(local_row));
        const auto full_row = static_cast<std::uint32_t>(bank * smesh::kSpBankRows) + local_row;

        smesh::SpadReadResp response{};
        response.laddr = smesh::makeSpAddr(full_row);
        response.mask = static_cast<std::uint8_t>((1u << smesh::kDim) - 1u);
        response.len = smesh::kDim;
        response.from_dma = req_bits[bank]->from_dma;
        for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
          response.data[lane] = static_cast<smesh::Elem>(full_row * smesh::kDim + lane);
        }
        next.bits[bank][0] = response;

        trace(ex_ctrl_spad_mem_view,
              "req  bank=%u row=%u data={%d,%d,%d,%d}\n",
              static_cast<unsigned>(bank),
              static_cast<unsigned>(full_row),
              static_cast<int>(response.data[0]), static_cast<int>(response.data[1]),
              static_cast<int>(response.data[2]), static_cast<int>(response.data[3]));
      }
    }

    if (current.valid[bank][last] != 0 && resp_rdy[bank] != 0) {
      const auto& response = current.bits[bank][last];
      trace(ex_ctrl_spad_mem_view,
            "take  bank=%u row=%u data={%d,%d,%d,%d}\n",
            static_cast<unsigned>(bank),
            static_cast<unsigned>(response.laddr.full_sp_addr()),
            static_cast<int>(response.data[0]), static_cast<int>(response.data[1]),
            static_cast<int>(response.data[2]), static_cast<int>(response.data[3]));
    }
  }

  pipeline_ = next;
}

void ExCtrlSpadModel::reset() {
  active_reg_.reset(0);
  pipeline_.reset(ExCtrlSpadPipelineState{});
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    req_rdy[bank].reset(0);
    resp_val[bank].reset(0);
    resp_bits[bank].reset(smesh::SpadReadResp{});
  }
}
// ********* SPAD *********
// ************************

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
  InputArray(bit, spad_read_req_rdy, smesh::kSpBanks);
  InputArray(bit, spad_read_req_val, smesh::kSpBanks);
  InputArray(smesh::SpadBankReadReq, spad_read_req_bits, smesh::kSpBanks);
  InputArray(bit, spad_read_resp_val, smesh::kSpBanks);
  InputArray(bit, spad_read_resp_rdy, smesh::kSpBanks);
  InputArray(smesh::SpadReadResp, spad_read_resp_bits, smesh::kSpBanks);
  OutputArray(bit, accum_read_req_rdy, smesh::kAccBanks);
  InputArray(bit, accum_read_req_val, smesh::kAccBanks);
  OutputArray(bit, accum_read_resp_val, smesh::kAccBanks);
  OutputArray(smesh::ExCtrlAccumReadResp, accum_read_resp_bits, smesh::kAccBanks);
  OutputArray(bit, spad_write_rdy, smesh::kSpBanks);
  OutputArray(bit, accum_write_rdy, smesh::kAccBanks);

  void update_memory_ready() {
    for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
      accum_read_req_rdy[bank] = 1;
      accum_read_resp_val[bank] = 0;
      accum_read_resp_bits[bank] = smesh::ExCtrlAccumReadResp{};
      accum_write_rdy[bank] = 1;
    }
    for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
      spad_write_rdy[bank] = 1;
    }
  }

  void update_issue() {
    if (Sim::state == Sim::SimResetting || next_issue_ >= kScenario.program.size() || cmd_out.full()) {
      return;
    }

    cmd_out.push(kScenario.program[next_issue_]);
    ++next_issue_;
  }

  void update_completion() {
    if (Sim::state == Sim::SimResetting) {
      return;
    }
    // emit TraceKey with s_trace
    s_trace(ex_ctrl_view,
          "state=%4s h0{v=%u t=%03u c=%4s} h1{v=%u t=%03u c=%4s} h2{v=%u t=%03u c=%4s}\n",
            stateName(static_cast<std::uint8_t>(*control_state)),
            static_cast<unsigned>(head_val[0]), static_cast<unsigned>(head_bits[0]->rs_tag), commandName(head_val[0] != 0, head_bits[0]->cmd.funct),
            static_cast<unsigned>(head_val[1]), static_cast<unsigned>(head_bits[1]->rs_tag), commandName(head_val[1] != 0, head_bits[1]->cmd.funct),
            static_cast<unsigned>(head_val[2]), static_cast<unsigned>(head_bits[2]->rs_tag), commandName(head_val[2] != 0, head_bits[2]->cmd.funct));
    for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
      if (head_val[i] == 0) {
        continue;
      }
      for (std::size_t j = 0; j < kScenario.program.size(); ++j) {
        if (head_bits[i]->rs_tag == kScenario.program[j].rs_tag &&
            head_bits[i]->cmd.funct == kScenario.program[j].cmd.funct) {
          seen_[j] = true;
        }
      }
    }
    if (!rowaddr_checked_ &&
        *control_state == static_cast<std::uint8_t>(smesh::ExCtrlFsmState::Compute)) {
      rowaddr_checked_ = true;
      rowaddr_matched_ = rowaddr_a_address->data() == kScenario.expected.rowaddr_a_address &&
                         rowaddr_b_address->data() == kScenario.expected.rowaddr_b_address &&
                         rowaddr_d_address->data() == kScenario.expected.rowaddr_d_address &&
                         *rowaddr_a_bank == smesh::makeSpAddr(kScenario.expected.rowaddr_a_address).sp_bank() &&
                         *rowaddr_b_bank == smesh::makeSpAddr(kScenario.expected.rowaddr_b_address).sp_bank() &&
                         *rowaddr_d_bank == smesh::makeSpAddr(kScenario.expected.rowaddr_d_address).sp_bank() &&
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
      const auto expected = smesh::makeSpAddr(kScenario.expected.first_read_address);
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

    // Record the first standalone-PRELOAD request/response sequence. Later
    // requests are intentionally ignored until the FSM termination step exists.
    for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
      if (spad_read_req_val[bank] != 0 && spad_read_req_rdy[bank] != 0 &&
          preload_req_count_ < smesh::kDim) {
        const auto full_row = static_cast<std::uint32_t>(bank * smesh::kSpBankRows) +
                              static_cast<std::uint32_t>(spad_read_req_bits[bank]->addr);
        preload_memory_matched_ &=
            full_row == kScenario.expected.preload_read_addresses[preload_req_count_];
        preload_req_cycle_[preload_req_count_] = cycle_;
        ++preload_req_count_;
      }

      if (spad_read_resp_val[bank] != 0 && spad_read_resp_rdy[bank] != 0 &&
          preload_resp_count_ < smesh::kDim) {
        const auto& response = *spad_read_resp_bits[bank];
        const auto expected_row = kScenario.expected.preload_read_addresses[preload_resp_count_];
        preload_memory_matched_ &= response.laddr.full_sp_addr() == expected_row;
        preload_memory_matched_ &= response.from_dma == 0;
        for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
          const auto expected_value =
              static_cast<smesh::Elem>(expected_row * smesh::kDim + lane);
          preload_memory_matched_ &= response.data[lane] == expected_value;
        }
        ++preload_resp_count_;
      }

      if (!first_resp_available_checked_ && spad_read_resp_val[bank] != 0) {
        first_resp_available_checked_ = true;
        first_resp_available_matched_ =
            spad_read_resp_bits[bank]->laddr.full_sp_addr() ==
                kScenario.expected.preload_read_addresses[0] &&
            cycle_ - preload_req_cycle_[0] == static_cast<int>(smesh::kSpadReadDelay);
      }
    }

    if (preload_req_count_ != 0 && preload_resp_count_ < smesh::kDim) {
      for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
        unexpected_accum_read_ |=
            accum_read_req_val[bank] != 0 && accum_read_req_rdy[bank] != 0;
      }
    }

    const bool early_pipeline_matched =
        seen_[0] && seen_[1] && seen_[2] &&
        rowaddr_checked_ && rowaddr_matched_ &&
        rowpad_checked_ && rowpad_matched_ &&
        read_req_checked_ && read_req_matched_;
    const bool preload_memory_complete =
        preload_req_count_ == smesh::kDim && preload_resp_count_ == smesh::kDim;
    matched_ = early_pipeline_matched && preload_memory_complete &&
               preload_memory_matched_ && first_resp_available_checked_ &&
               first_resp_available_matched_ && !unexpected_accum_read_;
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
    preload_req_cycle_ = {};
    preload_req_count_ = 0;
    preload_resp_count_ = 0;
    preload_memory_matched_ = true;
    first_resp_available_checked_ = false;
    first_resp_available_matched_ = false;
    unexpected_accum_read_ = false;
    done_ = false;
    matched_ = false;
    cycle_ = 0;
  }

  bool done() const { return done_; }
  bool matched() const { return matched_; }
  void reportFailure() const {
    std::printf(
        "  early{seen=%u%u%u addr=%u pad=%u read=%u} "
        "spad{req=%zu resp=%zu seq=%u first_offer=%u} accum_read=%u\n",
        static_cast<unsigned>(seen_[0]), static_cast<unsigned>(seen_[1]),
        static_cast<unsigned>(seen_[2]), static_cast<unsigned>(rowaddr_matched_),
        static_cast<unsigned>(rowpad_matched_), static_cast<unsigned>(read_req_matched_),
        preload_req_count_, preload_resp_count_,
        static_cast<unsigned>(preload_memory_matched_),
        static_cast<unsigned>(first_resp_available_matched_),
        static_cast<unsigned>(unexpected_accum_read_));
  }

 private:
  int cycle_ = 0;
  std::size_t next_issue_ = 0;
  std::array<bool, 3> seen_{};
  bool rowaddr_checked_ = false;
  bool rowaddr_matched_ = false;
  bool rowpad_checked_ = false;
  bool rowpad_matched_ = false;
  bool read_req_checked_ = false;
  bool read_req_matched_ = false;
  std::array<int, smesh::kDim> preload_req_cycle_{};
  std::size_t preload_req_count_ = 0;
  std::size_t preload_resp_count_ = 0;
  bool preload_memory_matched_ = true;
  bool first_resp_available_checked_ = false;
  bool first_resp_available_matched_ = false;
  bool unexpected_accum_read_ = false;
  bool done_ = false;
  bool matched_ = false;
};

ExCtrlDriver::ExCtrlDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update_issue).writes(cmd_out);
  UPDATE(update_memory_ready)
      .writes(accum_read_req_rdy, accum_read_resp_val, accum_read_resp_bits,
              spad_write_rdy, accum_write_rdy);
  UPDATE(update_completion).reads(control_state, config_val, config_rs_tag_valid, config_rs_tag,
                                  head_val, head_bits)
                           .reads(rowaddr_a_address, rowaddr_b_address, rowaddr_d_address,
                                  rowaddr_a_bank, rowaddr_b_bank, rowaddr_d_bank)
                           .reads(rowaddr_a_garbage, rowaddr_b_garbage, rowaddr_d_garbage)
                           .reads(rowpad_a_row_not_zero, rowpad_b_row_not_zero, rowpad_d_row_not_zero,
                                  rowpad_a_unpadded_cols, rowpad_b_unpadded_cols,
                                  rowpad_d_unpadded_cols)
                           .reads(spad_read_req_val, spad_read_req_rdy, spad_read_req_bits,
                                  spad_read_resp_val, spad_read_resp_rdy, spad_read_resp_bits,
                                  accum_read_req_val, accum_read_req_rdy);
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::ExCtrl ctrl("ExCtrl");
  ExCtrlDriver driver("Driver");
  ExCtrlSpadModel spad("SpadModel");

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
    spad.req_val[bank] << ctrl.spad_read_req_val[bank];
    spad.req_bits[bank] << ctrl.spad_read_req_bits[bank];
    ctrl.spad_read_req_rdy[bank] << spad.req_rdy[bank];
    ctrl.spad_read_resp_val[bank] << spad.resp_val[bank];
    ctrl.spad_read_resp_bits[bank] << spad.resp_bits[bank];
    spad.resp_rdy[bank] << ctrl.spad_read_resp_rdy[bank];

    driver.spad_read_req_rdy[bank] << spad.req_rdy[bank];
    driver.spad_read_req_val[bank] << ctrl.spad_read_req_val[bank];
    driver.spad_read_req_bits[bank] << ctrl.spad_read_req_bits[bank];
    driver.spad_read_resp_val[bank] << spad.resp_val[bank];
    driver.spad_read_resp_rdy[bank] << ctrl.spad_read_resp_rdy[bank];
    driver.spad_read_resp_bits[bank] << spad.resp_bits[bank];
    ctrl.spad_write_rdy[bank] << driver.spad_write_rdy[bank];
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    ctrl.accum_read_req_rdy[bank] << driver.accum_read_req_rdy[bank];
    driver.accum_read_req_val[bank] << ctrl.accum_read_req_val[bank];
    ctrl.accum_read_resp_val[bank] << driver.accum_read_resp_val[bank];
    ctrl.accum_read_resp_bits[bank] << driver.accum_read_resp_bits[bank];
    ctrl.accum_write_rdy[bank] << driver.accum_write_rdy[bank];
  }
  for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
    driver.head_val[i] << ctrl.cmd_queue_head_val[i];
    driver.head_bits[i] << ctrl.cmd_queue_head_bits[i];
  }
  ctrl.cmd_in.setDelay(1);

  Clock clk;
  ctrl.clk << clk;
  driver.clk << clk;
  spad.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 32 && !driver.done(); ++i) {
    Sim::run();
  }

  const bool ok = driver.done() && driver.matched();
  std::printf("[EX_CTRL] %s config_preload_compute_spad_request_response\n",
              ok ? "PASS" : "FAIL");
  if (!ok) {
    driver.reportFailure();
  }
  descore::flushLog(); // flush log before exiting because trace o/p is buffered
  return ok ? 0 : 1;
}
