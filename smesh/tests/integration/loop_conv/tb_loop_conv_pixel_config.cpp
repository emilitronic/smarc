// **********************************************************************
// smesh/tests/integration/loop_conv/tb_loop_conv_pixel_config.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 18 2026
/*
Check LOOP_CONV_WS -> CONFIG_LOAD -> LdCtrl DMA requests with a requested pixel
repeat count of two. A test-only RS stand-in forwards load commands to LdCtrl,
assigns tags, and returns LoopConv credits. A DMA stand-in acknowledges requests
one cycle after acceptance. This checks command/configuration propagation;
numerical execution and local-memory writes belong to the rs_to_mem suite.

ctest --test-dir build -R loop_conv_pixel_config --output-on-failure
./build/smesh/tb_loop_conv_pixel_config_enabled -trace '*'
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LdCtrl.hpp"
#include "LoopConv.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>
#include <set>

namespace {

constexpr std::uint64_t kA = 0x80004000;
constexpr std::uint64_t kB = 0x80005000;
constexpr std::uint64_t kD = 0x80006000;
constexpr std::uint64_t kC = 0x80007000;
static_assert(smesh::kDefaultConfig.has_first_layer_optimizations == (SMESH_TEST_EXPECT_FIRST_LAYER != 0),
              "The test must link the expected first-layer model configuration");
constexpr unsigned kPixels = SMESH_TEST_EXPECT_FIRST_LAYER ? 2 : 1;

smesh::SmeshQueuedCmd command(smesh::SmeshFunct funct, std::uint64_t rs1, std::uint64_t rs2) {
  smesh::SmeshQueuedCmd value{};
  value.cmd.funct = static_cast<unsigned>(funct);
  value.cmd.rs1 = rs1;
  value.cmd.rs2 = rs2;
  return value;
}

// Queue classes used by the small RS stand-in; ordinary trailing commands use 3.
unsigned group(const smesh::SmeshCmd& cmd) {
  const auto funct = static_cast<smesh::SmeshFunct>(static_cast<unsigned>(cmd.funct));
  if (funct == smesh::SmeshFunct::Config) {
    const auto kind = static_cast<smesh::ConfigKind>(static_cast<std::uint64_t>(cmd.rs1) & 3u);
    return kind == smesh::ConfigKind::Load ? 0 : kind == smesh::ConfigKind::Execute ? 1 : 2;
  }
  if (funct == smesh::SmeshFunct::Mvin || funct == smesh::SmeshFunct::Mvin2 ||
      funct == smesh::SmeshFunct::Mvin3) return 0;
  if (funct == smesh::SmeshFunct::Preload || funct == smesh::SmeshFunct::ComputeFlip ||
      funct == smesh::SmeshFunct::ComputeStay) return 1;
  if (funct == smesh::SmeshFunct::Mvout) return 2;
  return 3;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, in_val);
  Output(smesh::SmeshQueuedCmd, in_bits);
  Input(bit, in_rdy);
  Input(bit, out_val);
  Input(smesh::SmeshQueuedCmd, out_bits);
  Output(bit, out_rdy);
  Output(bit, load_cmd_val);
  Output(smesh::SmeshIssue, load_cmd_bits);
  Input(bit, load_cmd_rdy);
  Input(bit, dma_req_val);
  Input(smesh::DmaReadReq, dma_req_bits);
  Output(bit, dma_req_rdy);
  Output(bit, dma_resp_val);
  Register(bit, dma_resp_val_D_);
  Output(smesh::DmaReadCompletion, dma_resp_bits);
  Register(smesh::DmaReadCompletion, dma_resp_bits_D_);
  Input(bit, completed_val);
  Input(smesh::SmeshRsTag, completed_bits);
  Output(u8, ld_completed);
  Output(u8, ex_completed);
  Output(u8, st_completed);
  Register(u8, ld_completed_D_);
  Register(u8, ex_completed_D_);
  Register(u8, st_completed_D_);
  Input(smesh::LoopConvSlot, loop0);
  Input(bit, loop_busy);
  Input(bit, load_busy);
  Input(u16, ld_outstanding);
  Input(u16, ex_outstanding);
  Input(u16, st_outstanding);
  void updateDrive();
  void updateIssue();
  void updateReady();
  void updateState();
  void reset();
  bool passed() const;

 private:
  const std::array<smesh::SmeshQueuedCmd, 8> program_;
  Output(u16, position_Q_);
  Register(u16, position_D_);
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
  Output(u16, load_tag_Q_);
  Register(u16, load_tag_D_);
  // Simulation-only observations. These do not drive either controller.
  std::array<unsigned, 3> configs_{};
  std::array<unsigned, 3> loads_{};
  unsigned input_rows_ = 0;
  unsigned weight_rows_ = 0;
  unsigned bias_rows_ = 0;
  unsigned preloads_ = 0;
  unsigned computes_ = 0;
  unsigned completions_ = 0;
  std::set<smesh::SmeshRsTag> pending_tags_;
  bool saw_slot_ = false;
  bool saw_cmd_stall_ = false;
  bool saw_dma_stall_ = false;
  bool trailing_seen_ = false;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR)
    : program_{{
        command(smesh::SmeshFunct::LoopConvWsConfig1,
                (2ull << 48) | (2ull << 32) | (1ull << 16) | 1,
                (1ull << 48) | (4ull << 32) | 1),
        command(smesh::SmeshFunct::LoopConvWsConfig2, 3ull << 48, (1ull << 48) | 2),
        command(smesh::SmeshFunct::LoopConvWsConfig3,
                (1ull << 48) | (3ull << 32) | (2ull << 16), 6),
        command(smesh::SmeshFunct::LoopConvWsConfig4, (1ull << 48) | 1,
                (2ull << 48) | (2ull << 32) | (2ull << 16) | 4),
        command(smesh::SmeshFunct::LoopConvWsConfig5, kB, kC),
        command(smesh::SmeshFunct::LoopConvWsConfig6, kD, kA),
        // Two input channels and two output channels; request two pixels per row.
        command(smesh::SmeshFunct::LoopConvWs, (1ull << 18) | (2ull << 16) | (2ull << 8), 1),
        command(smesh::SmeshFunct::Flush, 0x1234, 0),
      }} {
  static_assert(smesh::kDim == 4, "This control-path example uses a 4x4 array");
  position_Q_ <= position_D_;
  cycle_Q_ <= cycle_D_;
  load_tag_Q_ <= load_tag_D_;
  dma_resp_val <= dma_resp_val_D_;
  dma_resp_bits <= dma_resp_bits_D_;
  ld_completed <= ld_completed_D_;
  ex_completed <= ex_completed_D_;
  st_completed <= st_completed_D_;
  UPDATE(updateDrive).reads(position_Q_, cycle_Q_).writes(in_val, in_bits, dma_req_rdy);
  UPDATE(updateIssue).reads(out_val, out_bits, load_tag_Q_, cycle_Q_)
      .writes(load_cmd_val, load_cmd_bits);
  UPDATE(updateReady).reads(load_cmd_rdy, cycle_Q_).writes(out_rdy);
  UPDATE(updateState).reads(position_Q_, cycle_Q_, in_val, in_rdy, out_val, out_bits, out_rdy)
      .reads(load_tag_Q_, dma_req_val, dma_req_bits, dma_req_rdy, completed_val, completed_bits, loop0)
      .writes(position_D_, cycle_D_, load_tag_D_, dma_resp_val_D_, dma_resp_bits_D_)
      .writes(ld_completed_D_, ex_completed_D_, st_completed_D_);
}

// Drive the macro commands and periodically stall DMA request acceptance.
void Driver::updateDrive() {
  const unsigned position = static_cast<unsigned>(*position_Q_);
  const unsigned cycle = static_cast<unsigned>(*cycle_Q_);
  const bool active = Sim::state != Sim::SimResetting;
  in_val = bit(active && position < program_.size());
  in_bits = position < program_.size() ? program_[position] : smesh::SmeshQueuedCmd{};
  dma_req_rdy = bit(active && cycle >= 15 && cycle % 4 >= 2);
}

// Forward generated load commands to the real LdCtrl with a test ROB tag.
void Driver::updateIssue() {
  smesh::SmeshIssue issue{};
  issue.cmd = out_bits->cmd;
  issue.rs_tag = static_cast<smesh::SmeshRsTag>(*load_tag_Q_);
  load_cmd_val = bit(out_val == 1 && group(issue.cmd) == 0 &&
                     cycle_Q_ >= 12 && static_cast<unsigned>(*cycle_Q_) % 3 != 1);
  load_cmd_bits = issue;
}

void Driver::updateReady() {
  const bool allowed = cycle_Q_ >= 12 && static_cast<unsigned>(*cycle_Q_) % 3 != 1;
  // Conservatively stall all workers when the load-command queue is full.
  // Ready depends on queue capacity, independently of the selected command bits.
  out_rdy = bit(allowed && load_cmd_rdy == 1);
}

// Check the configuration at each public boundary and return delayed credits.
void Driver::updateState() {
  if (Sim::state == Sim::SimResetting) return;
  cycle_D_ = u16(static_cast<unsigned>(*cycle_Q_) + 1);
  if (in_val == 1 && in_rdy == 1) position_D_ = u16(static_cast<unsigned>(*position_Q_) + 1);
  dma_resp_val_D_ = 0;
  dma_resp_bits_D_ = smesh::DmaReadCompletion{};
  unsigned load_credits = 0;
  ex_completed_D_ = 0;
  st_completed_D_ = 0;

  if (loop0->configured == 1) {
    assert_always(loop0->launch.max_pixels_per_row == kPixels,
                  "LoopConv did not retain the configured pixel count");
    saw_slot_ = true;
  }
  if (completed_val == 1) {
    const auto removed = pending_tags_.erase(*completed_bits);
    assert_always(removed == 1,
                  "LdCtrl returned a duplicate or unexpected completion");
    ++completions_;
    ++load_credits;
  }
  if (out_val == 1) {
    const auto cmd = out_bits->cmd;
    const unsigned kind = group(cmd);
    const auto funct = static_cast<smesh::SmeshFunct>(static_cast<unsigned>(cmd.funct));
    const auto rs1 = static_cast<std::uint64_t>(cmd.rs1);
    if (kind == 0 && funct == smesh::SmeshFunct::Config) {
      const unsigned state = smesh::unpackConfigStateId(rs1);
      assert_always(state < 3 && ((rs1 >> 8) & 0xffu) == (state == 0 ? kPixels : 1),
                    "LoopConv emitted an incorrect CONFIG_LOAD pixel repeat count");
      if (out_rdy == 0) saw_cmd_stall_ = true;
    }
    if (out_rdy == 1) {
      if (kind == 0) {
        load_tag_D_ = u16(static_cast<unsigned>(*load_tag_Q_) + 1);
        if (funct == smesh::SmeshFunct::Config) {
          const unsigned state = smesh::unpackConfigStateId(rs1);
          ++configs_[state];
          ++load_credits; // RS completes load CONFIG commands on issue.
          trace("pixel_config: CONFIG_LOAD state=%u pixel_repeats=%u\n",
                state, unsigned((rs1 >> 8) & 0xffu));
        } else {
          const unsigned state = funct == smesh::SmeshFunct::Mvin ? 0 :
                                 funct == smesh::SmeshFunct::Mvin2 ? 1 : 2;
          ++loads_[state];
          const auto inserted = pending_tags_.insert(static_cast<smesh::SmeshRsTag>(*load_tag_Q_));
          assert_always(inserted.second,
                        "RS stand-in reused a pending command tag");
        }
      } else if (kind == 1) {
        ex_completed_D_ = 1;
        const unsigned expected_k = kPixels == 2 && preloads_ == 0 ? 4 : 2;
        if (funct == smesh::SmeshFunct::Preload) {
          assert_always(smesh::unpackLocal(rs1).shape.rows == expected_k,
                        "LoopConv Execute did not group the requested kernel pixels");
          ++preloads_;
        } else {
          ++computes_;
        }
      } else if (kind == 2) {
        st_completed_D_ = 1;
      } else {
        assert_always(funct == smesh::SmeshFunct::Flush && rs1 == 0x1234,
                      "Unexpected command after loop generation");
        trailing_seen_ = true;
      }
    }
  }

  if (dma_req_val == 1) {
    const auto req = *dma_req_bits;
    const auto address = static_cast<std::uint64_t>(req.vaddr);
    const bool input = address >= kA && address < kA + 12;
    const bool weights = address >= kB && address < kB + 12;
    assert_always(req.pixel_repeats == (input ? kPixels : 1) && req.cols == 2 &&
                      req.scale == smesh::kMvinScaleIdentityBits && req.all_zeros == 0,
                  "LdCtrl changed the load's repeat count or basic payload");
    if (input) {
      assert_always(address == kA + input_rows_ * 2 &&
                        req.laddr.raw == smesh::makeSpAddr(input_rows_).raw &&
                        req.block_stride == 6 && req.repeats == 0 && req.has_acc_bitwidth == 0,
                    "Input DMA request changed while stalled or used the wrong configuration");
    }
    if (dma_req_rdy == 0) saw_dma_stall_ = true;
    if (dma_req_rdy == 1) {
      if (input) ++input_rows_;
      else if (weights) ++weight_rows_;
      else {
        assert_always(address == kD && req.has_acc_bitwidth == 1 && req.repeats == 3,
                      "Unexpected bias DMA request");
        ++bias_rows_;
      }
      smesh::DmaReadCompletion response{};
      response.cmd_id = req.cmd_id;
      response.bytes_read = u16(static_cast<unsigned>(req.cols) *
                                (req.has_acc_bitwidth == 1 ? sizeof(smesh::Acc) : sizeof(smesh::Elem)));
      dma_resp_val_D_ = 1;
      dma_resp_bits_D_ = response;
      trace("pixel_config: DMA addr=0x%llx pixel_repeats=%u bytes=%u\n",
            static_cast<unsigned long long>(address), unsigned(req.pixel_repeats),
            unsigned(response.bytes_read));
    }
  }
  ld_completed_D_ = u8(load_credits);
}

void Driver::reset() {
  position_D_.reset(0);
  cycle_D_.reset(0);
  load_tag_D_.reset(0);
  dma_resp_val_D_.reset(0);
  dma_resp_bits_D_.reset(smesh::DmaReadCompletion{});
  ld_completed_D_.reset(0);
  ex_completed_D_.reset(0);
  st_completed_D_.reset(0);
  in_val.reset(0);
  in_bits.reset(smesh::SmeshQueuedCmd{});
  out_rdy.reset(0);
  load_cmd_val.reset(0);
  load_cmd_bits.reset(smesh::SmeshIssue{});
  dma_req_rdy.reset(0);
  configs_ = {};
  loads_ = {};
  input_rows_ = weight_rows_ = bias_rows_ = preloads_ = computes_ = completions_ = 0;
  pending_tags_.clear();
  saw_slot_ = saw_cmd_stall_ = saw_dma_stall_ = trailing_seen_ = false;
}

bool Driver::passed() const {
  return position_Q_ == program_.size() && load_tag_Q_ == 9 &&
         configs_ == std::array<unsigned, 3>{{1, 1, 1}} &&
         loads_ == std::array<unsigned, 3>{{2, 3, 1}} && input_rows_ == 6 &&
         weight_rows_ == 6 && bias_rows_ == 1 && completions_ == 6 && pending_tags_.empty() &&
         preloads_ == (kPixels == 2 ? 2u : 3u) && computes_ == preloads_ &&
         saw_slot_ && saw_cmd_stall_ && saw_dma_stall_ && trailing_seen_ &&
         loop_busy == 0 && load_busy == 0 && ld_outstanding == 0 &&
         ex_outstanding == 0 && st_outstanding == 0;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver driver("Driver");
  smesh::LoopConv loop("LoopConv");
  smesh::LdCtrl load("LdCtrl");
  loop.in_val << driver.in_val;
  loop.in_bits << driver.in_bits;
  driver.in_rdy << loop.in_rdy;
  driver.out_val << loop.out_val;
  driver.out_bits << loop.out_bits;
  loop.out_rdy << driver.out_rdy;
  load.cmd_val << driver.load_cmd_val;
  load.cmd_bits << driver.load_cmd_bits;
  driver.load_cmd_rdy << load.cmd_rdy;
  driver.dma_req_val << load.dma_req_val;
  driver.dma_req_bits << load.dma_req_bits;
  load.dma_req_rdy << driver.dma_req_rdy;
  load.dma_resp_val << driver.dma_resp_val;
  load.dma_resp_bits << driver.dma_resp_bits;
  driver.completed_val << load.completed_val;
  driver.completed_bits << load.completed_bits;
  load.completed_rdy.wireToConst(bit(1));
  loop.ld_completed << driver.ld_completed;
  loop.ex_completed << driver.ex_completed;
  loop.st_completed << driver.st_completed;
  driver.loop0 << loop.loop0;
  driver.loop_busy << loop.busy;
  driver.load_busy << load.busy;
  driver.ld_outstanding << loop.ld_outstanding;
  driver.ex_outstanding << loop.ex_outstanding;
  driver.st_outstanding << loop.st_outstanding;
  Clock clk;
  driver.clk << clk;
  loop.clk << clk;
  load.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  bool pass = true;
  for (unsigned run = 0; run < 2; ++run) {
    Sim::reset();
    for (unsigned cycle = 0; cycle < 220; ++cycle) Sim::run();
    pass &= driver.passed();
  }
  descore::flushLog();
  std::printf("[LOOP_CONV_PIXEL_CONFIG] %s first_layer=%u requested=2 effective=%u\n",
              pass ? "PASS" : "FAIL", unsigned(smesh::kDefaultConfig.has_first_layer_optimizations), kPixels);
  return pass ? 0 : 1;
}
