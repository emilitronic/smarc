// **********************************************************************
// smesh/tests/unit/accum_response/tb_acc_scale_finite.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 9 2026
/*
Exercise the finite scaler through its external row ports. Seven rows wrap the
three slots twice. Check scaled rows in input order, source/bank metadata,
backpressure from the selected consumer, full-slot simultaneous pop/push,
unchanged full_data, mixed ordinary/ReLU/LayerNorm rows, the capability
flag, and reset while rows and elements are in flight.
*/
// cmake --build build --target tb_acc_scale_finite tb_acc_scale_finite_dim8 -j 4
// ./build/smesh/tb_acc_scale_finite -trace '*'/acc_scale_finite_view_

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "AccScaleFinite.hpp"

#include <cstdio>

TraceKey(acc_scale_finite_view_);

namespace {

BoolParameter(nonlinear, true, "Enable ReLU in scale pipes");
BoolParameter(normalizations, true, "Enable normalization-capable lanes");
constexpr unsigned kRows = 7;

smesh::AccScaleReq packet(unsigned id) {
  smesh::AccScaleReq p{};
  auto& row = p.norm.acc_read_resp;
  row.scale = id % 3 == 0 ? 0x3f000000u : id % 3 == 1 ? 0x40000000u : 0x3f800000u;
  row.act = u8(id % 2); // alternate ordinary scaling and ReLU
  if (normalizations && (id == 1 || id == 4)) {
    row.act = 2;
    p.norm.mean = id == 1 ? 5 : 7;
    p.norm.inv_stddev = 0x3f000000u; // effective 0.5 factor; ignore the ordinary 2.0 scale
  }
  const int n = static_cast<int>(id);
  const int half[] = {8 * n + 3, 8 * n + 5, -8 * n - 3, -8 * n - 5};
  const int twice[] = {100 + n, -65 - n, 63 - n, -64 + n};
  const int identity[] = {300 + n, -300 - n, 127 - n, -128 + n};
  for (std::size_t e = 0; e < smesh::kDim; ++e) {
    row.data[e] = id % 3 == 0 ? half[e % 4] : id % 3 == 1 ? twice[e % 4] : identity[e % 4];
  }
  row.from_dma = bit(id % 2 == 0);
  row.laddr = smesh::makeAccAddr((id % smesh::kAccBanks) * smesh::kAccBankRows + id);
  return p;
}

bool matches(const smesh::AccScaleResp& actual, unsigned id) {
  const auto source = packet(id).norm.acc_read_resp;
  const int n = static_cast<int>(id);
  const int half[] = {4 * n + 2, 4 * n + 2, -4 * n - 2, -4 * n - 2};
  const int twice[] = {127, -128, 126 - 2 * n, -128 + 2 * n};
  const int identity[] = {127, -128, 127 - n, -128 + n};
  if (actual.from_dma != source.from_dma || actual.acc_bank_id != source.laddr.acc_bank()) return false;
  for (std::size_t e = 0; e < smesh::kDim; ++e) {
    int scaled = id % 3 == 0 ? half[e % 4] : id % 3 == 1 ? twice[e % 4] : identity[e % 4];
    if (nonlinear && source.act == 1 && source.data[e] < 0) scaled = 0;
    if (nonlinear && source.act == 2) {
      constexpr int first_norm[] = {48, -36, 28, -34};
      constexpr int reused_norm[] = {48, -38, 26, -34};
      scaled = id == 1 ? first_norm[e % 4] : reused_norm[e % 4];
    }
    if (actual.full_data[e] != source.data[e] || actual.data[e] != scaled) return false;
  }
  return true;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, req_val);
  Output(smesh::AccScaleReq, req_bits);
  Output(bit, out_rdy_issue);
  Output(bit, out_rdy_exresp);
  Input(bit, req_rdy);
  Input(bit, out_val);
  Input(smesh::AccScaleResp, out_bits);
  void updateDrive();
  void updateCheck();
  void reset() override;
  bool passed = true;
  unsigned accepted = 0;
  unsigned returned = 0;
  bool saw_full = false;
  bool saw_reuse = false;
  bool saw_store_stall = false;
  bool saw_ex_stall = false;

 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  Output(u8, sent_Q_);
  Register(u8, sent_D_);
  Output(u8, seen_Q_);
  Register(u8, seen_D_);
  bool was_stalled_ = false; // monitor bookkeeping, not modeled hardware state
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  sent_Q_ <= sent_D_;
  seen_Q_ <= seen_D_;
  UPDATE(updateDrive).reads(cycle_Q_, sent_Q_)
                     .writes(req_val, req_bits, out_rdy_issue, out_rdy_exresp);
  UPDATE(updateCheck).reads(cycle_Q_, sent_Q_, seen_Q_, req_val, req_rdy, out_val, out_bits)
                     .reads(out_rdy_issue, out_rdy_exresp).writes(cycle_D_, sent_D_, seen_D_);
}

void Driver::updateDrive() {
  const unsigned c = static_cast<std::uint8_t>(*cycle_Q_);
  const unsigned sent = static_cast<std::uint8_t>(*sent_Q_);
  req_val = bit(sent < kRows);
  req_bits = packet(sent < kRows ? sent : 0);
  // Initially execute is ready but store is blocked; later reverse the situation.
  out_rdy_issue = bit(c >= 20 && c != 22 && c != 23);
  out_rdy_exresp = bit(c != 21 && c != 22);
}

void Driver::updateCheck() {
  const unsigned c = static_cast<std::uint8_t>(*cycle_Q_);
  const unsigned sent = static_cast<std::uint8_t>(*sent_Q_);
  const unsigned seen = static_cast<std::uint8_t>(*seen_Q_);
  const bool push = req_val == 1 && req_rdy == 1;
  const bool is_store = out_bits->from_dma == 1;
  const bool selected_ready = is_store ? out_rdy_issue == 1 : out_rdy_exresp == 1;
  const bool pop = out_val == 1 && selected_ready;
  bool good = sent >= seen && sent - seen <= 3;
  if (c < 7) good = good && out_val == 0; // four pipe cycles plus the row/arbOut/output registers
  if (was_stalled_) good = good && out_val == 1;
  if (out_val == 1) {
    good = good && seen < kRows && matches(*out_bits, seen);
    if (!selected_ready) {
      if (is_store && out_rdy_exresp == 1) saw_store_stall = true;
      if (!is_store && out_rdy_issue == 1) saw_ex_stall = true;
    }
  }
  was_stalled_ = out_val == 1 && !selected_ready;
  if (req_val == 1 && req_rdy == 0 && sent - seen == 3) saw_full = true;
  if (push && pop && sent - seen == 3) saw_reuse = true;
  if (c == 19) good = good && sent == 3 && seen == 0 && req_rdy == 0;
  if (push) {
    sent_D_ = u8(sent + 1);
    ++accepted;
  }
  if (pop) {
    seen_D_ = u8(seen + 1);
    ++returned;
    trace(acc_scale_finite_view_, "cycle=%02u output row=%u bank=%u from_dma=%u data0=%d\n",
          c, seen, static_cast<unsigned>(out_bits->acc_bank_id),
          static_cast<unsigned>(out_bits->from_dma), static_cast<int>(out_bits->data[0]));
  }
  if (!good) std::printf("[ACC_SCALE_FINITE] mismatch cycle=%u sent=%u seen=%u\n", c, sent, seen);
  passed = passed && good;
  cycle_D_ = u8(c + 1);
}

void Driver::reset() {
  cycle_Q_.reset(0);
  cycle_D_.reset(0);
  sent_Q_.reset(0);
  sent_D_.reset(0);
  seen_Q_.reset(0);
  seen_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::AccScaleReq{});
  out_rdy_issue.reset(0);
  out_rdy_exresp.reset(0);
  passed = true;
  accepted = returned = 0;
  saw_full = saw_reuse = saw_store_stall = saw_ex_stall = was_stalled_ = false;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  smesh::AccScaleFinite scale("FiniteScale", 4, nonlinear, normalizations);
  Driver driver("Driver");
  Clock clk;
  scale.clk << clk;
  driver.clk << clk;
  scale.req_val << driver.req_val;
  scale.req_bits << driver.req_bits;
  scale.out_rdy_issue << driver.out_rdy_issue;
  scale.out_rdy_exresp << driver.out_rdy_exresp;
  driver.req_rdy << scale.req_rdy;
  driver.out_val << scale.out_val;
  driver.out_bits << scale.out_bits;
  clk.generateClock();
  Sim::init();
  Sim::reset();
  for (unsigned c = 0; c < 9; ++c) Sim::run();
  bool good = driver.passed;
  Sim::reset(); // clear occupied slots and in-flight pipe elements
  good = good && scale.out_val == 0;
  for (unsigned c = 0; c < 64; ++c) Sim::run();
  good = good && driver.passed && driver.accepted == kRows && driver.returned == kRows &&
         driver.saw_full && driver.saw_reuse && driver.saw_store_stall && driver.saw_ex_stall;
  if (!good) {
    std::printf("[ACC_SCALE_FINITE] checks=%u full=%u reuse=%u store_stall=%u ex_stall=%u\n",
                driver.passed, driver.saw_full, driver.saw_reuse,
                driver.saw_store_stall, driver.saw_ex_stall);
  }
  std::printf("[ACC_SCALE_FINITE] width=%u activation=%u normalization=%u accepted=%u returned=%u %s\n",
              static_cast<unsigned>(smesh::kDim), static_cast<unsigned>(nonlinear),
              static_cast<unsigned>(normalizations), driver.accepted, driver.returned,
              good ? "PASS" : "FAIL");
  descore::flushLog();
  return good ? 0 : 1;
}
