// **********************************************************************
// smesh/tests/unit/st_ctrl/tb_st_ctrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
// Exercises store command retention, request timing, geometry, and completion.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "SmeshCommand.hpp"
#include "StCtrl.hpp"
#include "StCtrlState.hpp"

#include <array>
#include <cstdio>
#include <deque>

namespace {

smesh::SmeshIssue issue(smesh::SmeshFunct funct, std::uint16_t tag,
                        std::uint64_t rs1, std::uint64_t rs2) {
  smesh::SmeshIssue value{};
  value.cmd.funct = static_cast<std::uint32_t>(funct);
  value.cmd.rs1 = rs1;
  value.cmd.rs2 = rs2;
  value.rs_tag = tag;
  return value;
}

std::uint64_t configStoreRs1(std::uint8_t pool_stride, std::uint8_t pool_size,
                             std::uint8_t porows, std::uint8_t pocols,
                             std::uint8_t orows, std::uint8_t ocols) {
  return 2ull | (1ull << 2) | (std::uint64_t(pool_stride) << 4) |
         (std::uint64_t(pool_size) << 6) | (1ull << 24) |
         (std::uint64_t(porows) << 32) | (std::uint64_t(pocols) << 40) |
         (std::uint64_t(orows) << 48) | (std::uint64_t(ocols) << 56);
}

constexpr std::uint64_t configStoreRs2(std::uint32_t stride, std::uint32_t scale) {
  return (std::uint64_t(scale) << 32) | stride;
}

struct ExpectedReq {
  std::uint64_t vaddr;
  std::uint32_t local_row;
  std::uint16_t len;
  std::uint16_t cmd_id;
  std::uint8_t norm_cmd;
  bool dest_spad;
  bool pool_en;
  bool store_en;
};

constexpr std::array<ExpectedReq, 11> kExpectedReq{{
    {0x1000, 0, 4, 0, 1, false, false, false},
    {0x1000, 4, 2, 0, 2, false, false, true},
    {0x1010, 1, 4, 0, 1, false, false, false},
    {0x1010, 5, 2, 0, 2, false, false, true},
    {4, 1, 4, 1, 0, true, false, true},
    {7, 2, 4, 1, 0, true, false, true},
    {0x2000, 4, 4, 0, 0, false, false, false},
    {0x2000, 5, 4, 0, 0, false, true, false},
    {0x2000, 6, 4, 0, 0, false, true, false},
    {0x2000, 7, 4, 0, 0, false, true, true},
    {0x3000, 2, 4, 1, 0, false, false, true},
}};

class StoreDriver : public Component {
  DECLARE_COMPONENT(StoreDriver);

 public:
  StoreDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, cmd_val);
  Input(bit, cmd_rdy);
  Output(smesh::SmeshIssue, cmd_bits);
  Input(bit, dma_req_val);
  Input(smesh::DmaWriteReq, dma_req_bits);
  Output(bit, dma_req_rdy);
  Output(bit, dma_resp_val);
  Input(bit, dma_resp_rdy);
  Output(smesh::DmaWriteResp, dma_resp_bits);
  Input(bit, completed_val);
  Input(smesh::SmeshRsTag, completed_bits);
  Output(bit, completed_rdy);
  Input(u8, control_state);

  void update();
  void updateCommand();
  void reset();
  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  struct PendingResponse {
    std::uint16_t cmd_id;
    unsigned due_cycle;
  };

  std::array<smesh::SmeshIssue, 10> program_{{
      issue(smesh::SmeshFunct::Config, 1,
            configStoreRs1(0, 0, 0, 0, 0, 0), configStoreRs2(16, 0x1234)),
      issue(smesh::SmeshFunct::Config, 2,
            3ull | (3ull << 8) | (1ull << 16) | (0xaaaaull << 32),
            0x1111ull | (0x2222ull << 32)),
      issue(smesh::SmeshFunct::Mvout, 10, 0x1000,
            smesh::packLocal(smesh::makeAccAddr(0, false, false, 2), {2, 6})),
      issue(smesh::SmeshFunct::Config, 3,
            configStoreRs1(0, 2, 0, 0, 1, 2), configStoreRs2(32, 0xffffffffu)),
      issue(smesh::SmeshFunct::StoreSpad, 11,
            smesh::packStoreSpadDestination(smesh::makeSpAddr(4), 3),
            smesh::packLocal(smesh::makeSpAddr(1), {1, 4})),
      issue(smesh::SmeshFunct::Config, 4,
            configStoreRs1(1, 2, 1, 1, 2, 2), configStoreRs2(16, 0xffffffffu)),
      issue(smesh::SmeshFunct::Mvout, 12, 0x2000,
            smesh::packLocal(smesh::makeAccAddr(4), {1, 4})),
      issue(smesh::SmeshFunct::Config, 5,
            configStoreRs1(0, 0, 0, 0, 0, 0), configStoreRs2(16, 0xffffffffu)),
      issue(smesh::SmeshFunct::Config, 6,
            3ull | (7ull << 8) | (1ull << 17) | (0x9999ull << 32),
            0x3333ull | (0x4444ull << 32)),
      issue(smesh::SmeshFunct::Mvout, 13, 0x3000,
            smesh::packLocal(smesh::makeSpAddr(2), {1, 4})),
  }};
  std::deque<PendingResponse> pending_;
  std::size_t program_pos_ = 0;
  std::size_t req_pos_ = 0;
  std::size_t completion_pos_ = 0;
  std::array<unsigned, kExpectedReq.size()> stalls_{{0, 1, 0, 0, 2, 0, 1, 0, 0, 0, 0}};
  unsigned cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

StoreDriver::StoreDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateCommand).reads(cmd_rdy).writes(cmd_val, cmd_bits);
  UPDATE(update)
      .reads(dma_req_val, dma_req_bits, dma_resp_rdy,
             completed_val, completed_bits, control_state)
      .writes(dma_req_rdy, dma_resp_val, dma_resp_bits, completed_rdy);
}

void StoreDriver::updateCommand() {
  if (Sim::state == Sim::SimResetting || done_) {
    cmd_val = 0;
    cmd_bits = smesh::SmeshIssue{};
    return;
  }

  const bool command_offer = program_pos_ < program_.size();
  cmd_val = bit(command_offer);
  cmd_bits = command_offer ? program_[program_pos_] : smesh::SmeshIssue{};
  if (command_offer && cmd_rdy == 1) {
    ++program_pos_;
  }
}

void StoreDriver::update() {
  if (Sim::state == Sim::SimResetting || done_) {
    dma_req_rdy = 0;
    dma_resp_val = 0;
    dma_resp_bits = smesh::DmaWriteResp{};
    completed_rdy = 0;
    return;
  }

  const bool response_offer = !pending_.empty() && pending_.front().due_cycle <= cycle_;
  smesh::DmaWriteResp response{};
  if (response_offer) {
    response.cmd_id = pending_.front().cmd_id;
  }
  dma_resp_val = bit(response_offer);
  dma_resp_bits = response;
  completed_rdy = 1;

  const bool request_offer = dma_req_val == 1;
  const bool ready = req_pos_ < kExpectedReq.size() && stalls_[req_pos_] == 0;
  dma_req_rdy = bit(ready);
  if (request_offer && !ready && req_pos_ < kExpectedReq.size() && stalls_[req_pos_] != 0) {
    --stalls_[req_pos_];
  }

  if (request_offer && ready) {
    const auto req = *dma_req_bits;
    const auto& expected = kExpectedReq[req_pos_];
    const bool ok = req.vaddr == expected.vaddr &&
                    req.laddr.data() == expected.local_row &&
                    req.laddr.norm_cmd() == expected.norm_cmd &&
                    req.len == expected.len && req.cmd_id == expected.cmd_id &&
                    (req.dest == 1) == expected.dest_spad &&
                    (req.pool_en == 1) == expected.pool_en &&
                    (req.store_en == 1) == expected.store_en &&
                    req.acc_act == (req_pos_ < 4 ? 5 : 1) && req.acc_scale == 0x1234 &&
                    req.acc_igelu_qb == 0x1111 && req.acc_igelu_qc == 0x2222 &&
                    req.acc_iexp_qln2 == 0xaaaa &&
                    req.acc_norm_stats_id == (req_pos_ < 10 ? 3 : 7);
    if (!ok) {
      std::printf("[ST_CTRL2] request %zu mismatch: vaddr=0x%llx laddr=0x%x len=%u id=%u act=%u pool=%u store=%u state=%u\n",
                  req_pos_, static_cast<unsigned long long>(req.vaddr), req.laddr.raw,
                  static_cast<unsigned>(req.len), static_cast<unsigned>(req.cmd_id),
                  static_cast<unsigned>(req.acc_act), static_cast<unsigned>(req.pool_en == 1),
                  static_cast<unsigned>(req.store_en == 1), static_cast<unsigned>(*control_state));
    }
    passed_ &= ok;
    pending_.push_back({static_cast<std::uint16_t>(req.cmd_id), cycle_ + 3});
    ++req_pos_;
  }

  if (response_offer && dma_resp_rdy == 1) {
    pending_.pop_front();
  }
  if (completed_val == 1) {
    constexpr std::array<std::uint16_t, 4> kTags{{10, 11, 12, 13}};
    const bool ok = completion_pos_ < kTags.size() && *completed_bits == kTags[completion_pos_];
    if (!ok) {
      std::printf("[ST_CTRL2] completion %zu mismatch: tag=%u\n",
                  completion_pos_, static_cast<unsigned>(*completed_bits));
    }
    passed_ &= ok;
    ++completion_pos_;
  }

  ++cycle_;
  done_ = program_pos_ == program_.size() && req_pos_ == kExpectedReq.size() &&
          completion_pos_ == 4 && pending_.empty() && cycle_ > 12;
}

void StoreDriver::reset() {
  pending_.clear();
  program_pos_ = 0;
  req_pos_ = 0;
  completion_pos_ = 0;
  stalls_ = {{0, 1, 0, 0, 2, 0, 1, 0, 0, 0, 0}};
  cycle_ = 0;
  done_ = false;
  passed_ = true;
  cmd_val.reset(0);
  cmd_bits.reset(smesh::SmeshIssue{});
  dma_req_rdy.reset(0);
  dma_resp_val.reset(0);
  dma_resp_bits.reset(smesh::DmaWriteResp{});
  completed_rdy.reset(0);
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  StoreDriver driver("Driver");
  smesh::StCtrl controller("StCtrl");
  controller.cmd_val << driver.cmd_val;
  controller.cmd_bits << driver.cmd_bits;
  driver.cmd_rdy << controller.cmd_rdy;
  controller.dma_req_rdy << driver.dma_req_rdy;
  controller.dma_resp_val << driver.dma_resp_val;
  controller.dma_resp_bits << driver.dma_resp_bits;
  controller.completed_rdy << driver.completed_rdy;
  driver.dma_req_val << controller.dma_req_val;
  driver.dma_req_bits << controller.dma_req_bits;
  driver.dma_resp_rdy << controller.dma_resp_rdy;
  driver.completed_val << controller.completed_val;
  driver.completed_bits << controller.completed_bits;
  driver.control_state << controller.control_state;

  Clock clk;
  driver.clk << clk;
  controller.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 100 && !driver.done(); ++i) {
    Sim::run();
  }
  descore::flushLog();
  const bool ok = driver.done() && driver.passed();
  std::printf("[ST_CTRL2] %s store_fsm\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
