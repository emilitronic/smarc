// **********************************************************************
// smesh/tests/unit/mvin/tb_mvin_pixel_repeater.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 18 2026

// Independent pixel-repeater test. Expected writes below state the actual data,
// masks, and SPAD rows; the checker also observes stalls and register timing.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "MvinPixelRepeater.hpp"

#include <cstdio>
#include <initializer_list>
#include <vector>

namespace {

BoolParameter(passthrough, false, "Check the combinational passthrough configuration");

struct Scenario {
  std::vector<smesh::DmaReadResp> inputs;
  std::vector<smesh::DmaReadResp> outputs;
};

smesh::DmaReadResp packet(unsigned id, unsigned address, unsigned copies,
                         unsigned len, unsigned mask, bool last,
                         std::initializer_list<std::uint8_t> bytes) {
  smesh::DmaReadResp row{};
  row.laddr = smesh::makeSpAddr(address);
  row.pixel_repeats = u8(copies);
  row.len = u16(len);
  row.mask = mask;
  row.last = bit(last);
  row.cmd_id = u16(id);
  row.bytes_read = u16(len * sizeof(smesh::Elem));
  row.scale = 0x3f000000;
  row.repeats = 4; // Unrelated row-repeat metadata must survive unchanged.
  unsigned byte = 0;
  for (auto value : bytes) row.data[byte++] = value;
  return row;
}

smesh::DmaReadResp write(const smesh::DmaReadResp& input, unsigned address,
                        unsigned mask, bool last,
                        std::initializer_list<std::uint8_t> bytes) {
  auto row = input;
  row.laddr.raw = (input.laddr.raw & ~smesh::kLocalAddrDataMask) | address;
  row.mask = mask;
  row.last = bit(last);
  row.data = {};
  unsigned byte = 0;
  for (auto value : bytes) row.data[byte++] = value;
  return row;
}

Scenario scenario(bool bypass) {
  Scenario test;
  constexpr unsigned half = smesh::kSpRows / 2;

  // At row zero, the two preceding copies are skipped even while ready is low.
  auto row = packet(10, 0, 3, 1, 1, true, {9});
  test.inputs.push_back(row);
  test.outputs.push_back(write(row, 0, 1, true, {9}));

  // Two channels copied into the upper two positions of row 5, then row 6.
  row = packet(11, 6, 2, 2, 3, true, {11, 0xf2});
  test.inputs.push_back(row);
  test.outputs.push_back(write(row, 5, 0xc, false, {0, 0, 11, 0xf2}));
  test.outputs.push_back(write(row, 6, 3, true, {11, 0xf2}));

  // Do not let a copy from the second SPAD half enter the first half.
  row = packet(12, half, 2, 1, 1, true, {31});
  test.inputs.push_back(row);
  test.outputs.push_back(write(row, half, 1, true, {31}));

  // Three copies cross a bank boundary within a half; metadata stays attached.
  row = packet(13, smesh::kSpBankRows + 1, 3, 1, 1, false, {7});
  row.laddr.raw |= 2u << smesh::kLocalAddrNormShift;
  test.inputs.push_back(row);
  test.outputs.push_back(write(row, smesh::kSpBankRows - 1, 4, false, {0, 0, 7}));
  test.outputs.push_back(write(row, smesh::kSpBankRows, 2, false, {0, 7}));
  test.outputs.push_back(write(row, smesh::kSpBankRows + 1, 1, false, {7}));

  // Zero data still requires masked writes. This final marker accounts for a
  // whole ZeroWriter request, so its byte count must not change per copy.
  row = packet(14, half + 3, 2, 2, 3, true, {});
  row.bytes_read = 14;
  test.inputs.push_back(row);
  test.outputs.push_back(write(row, half + 2, 0xc, false, {}));
  test.outputs.push_back(write(row, half + 3, 3, true, {}));

  row = packet(15, 12, 1, 2, 2, true, {0x80, 0x7f});
  test.inputs.push_back(row);
  test.outputs.push_back(row);

  // Accumulator destinations can pass through the enabled block once, without
  // the SPAD address calculation or narrowing its wider packet storage.
  row = packet(16, 0, 1, smesh::kDim, (1u << smesh::kDim) - 1, true, {});
  row.laddr = smesh::makeAccAddr(3, false, true, 2);
  row.has_acc_bitwidth = 1;
  row.bytes_read = u16(smesh::kDim * sizeof(smesh::Acc));
  for (unsigned byte = 0; byte < row.data.size(); ++byte) row.data[byte] = byte + 1;
  test.inputs.push_back(row);
  test.outputs.push_back(row);

  // Fixed-width shifts discard data and mask bits that leave the row.
  row = packet(17, 4, 2, 2, (1u << smesh::kDim) - 1, true, {});
  for (unsigned lane = 0; lane < smesh::kDim; ++lane) row.data[lane] = lane + 1;
  test.inputs.push_back(row);
  if (smesh::kDim == 4)
    test.outputs.push_back(write(row, 3, 0xc, false, {0, 0, 1, 2}));
  else
    test.outputs.push_back(write(row, 3, 0xfc, false, {0, 0, 1, 2, 3, 4, 5, 6}));
  test.outputs.push_back(row);

  if (bypass) {
    for (auto& input : test.inputs) input.pixel_repeats = 1;
    test.outputs = test.inputs;
  }
  return test;
}

bool samePacket(const smesh::DmaReadResp& a, const smesh::DmaReadResp& b) {
  return a.data == b.data && a.mask == b.mask && a.laddr.raw == b.laddr.raw &&
         a.last == b.last && a.cmd_id == b.cmd_id && a.bytes_read == b.bytes_read &&
         a.len == b.len && a.scale == b.scale && a.repeats == b.repeats &&
         a.pixel_repeats == b.pixel_repeats && a.has_acc_bitwidth == b.has_acc_bitwidth;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, bool bypass, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, in_val);
  Output(smesh::DmaReadResp, in_bits);
  Input(bit, in_rdy);
  Input(bit, out_val);
  Input(smesh::DmaReadResp, out_bits);
  Output(bit, out_rdy);
  void updateInputs();
  void updateCheck();
  void reset();
  bool passed() const;

 private:
  const bool bypass_;
  const Scenario test_;
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
  Output(u16, sent_Q_);
  Register(u16, sent_D_);
  Output(u16, received_Q_);
  Register(u16, received_D_);
  bool stalled_ = false;
  bool reused_ = false;
  bool skipped_while_stalled_ = false;
  unsigned completion_bytes_ = 0;
};

Driver::Driver(std::string /*name*/, bool bypass, IMPL_CTOR)
    : bypass_(bypass), test_(scenario(bypass)) {
  static_assert(smesh::kDim == 4 || smesh::kDim == 8, "This test has golden rows for DIM=4 and DIM=8");
  cycle_Q_ <= cycle_D_;
  sent_Q_ <= sent_D_;
  received_Q_ <= received_D_;
  UPDATE(updateInputs).reads(cycle_Q_, sent_Q_).writes(in_val, in_bits, out_rdy);
  UPDATE(updateCheck).reads(cycle_Q_, sent_Q_, received_Q_, in_val, in_bits, in_rdy)
      .reads(out_val, out_bits, out_rdy).writes(cycle_D_, sent_D_, received_D_);
}

void Driver::updateInputs() {
  const unsigned sent = static_cast<unsigned>(*sent_Q_);
  const unsigned cycle = static_cast<unsigned>(*cycle_Q_);
  const bool active = Sim::state != Sim::SimResetting;
  in_val = bit(active && sent < test_.inputs.size());
  in_bits = sent < test_.inputs.size() ? test_.inputs[sent] : smesh::DmaReadResp{};
  out_rdy = bit(active && cycle >= 6 && cycle % 5 >= 2);
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const unsigned cycle = static_cast<unsigned>(*cycle_Q_);
  const unsigned sent = static_cast<unsigned>(*sent_Q_);
  const unsigned received = static_cast<unsigned>(*received_Q_);
  cycle_D_ = u16(cycle + 1);
  const bool push = in_val == 1 && in_rdy == 1;
  const bool pop = out_val == 1 && out_rdy == 1;
  if (push) sent_D_ = u16(sent + 1);

  if (bypass_) {
    assert_always(out_val == in_val && in_rdy == out_rdy,
                  "Passthrough introduced storage or changed the handshake");
  } else {
    if (sent == 0) assert_always(out_val == 0, "Pixel repetition did not register its input");
    if (cycle >= 3 && cycle < 6) {
      assert_always(out_val == 1 && out_bits->laddr.full_sp_addr() == 0,
                    "Copies below SPAD row zero did not skip while the consumer stalled");
      skipped_while_stalled_ = true;
    }
  }

  if (out_val == 1) {
    assert_always(received < test_.outputs.size(), "PixelRepeater duplicated an output packet");
    const auto& expected = test_.outputs[received];
    if (!bypass_) {
      const bool final_copy = received + 1 == test_.outputs.size() ||
                              test_.outputs[received + 1].cmd_id != expected.cmd_id;
      assert_always(in_rdy == bit(final_copy && out_rdy == 1),
                    "PixelRepeater input-ready did not match acceptance of the final copy");
    }
    assert_always(samePacket(*out_bits, expected),
                  "PixelRepeater changed the data, mask, address, or metadata of an expected write");
    if (out_rdy == 0) stalled_ = true;
    if (pop) {
      received_D_ = u16(received + 1);
      if (out_bits->last == 1) completion_bytes_ += unsigned(out_bits->bytes_read);
      if (push) reused_ = true;
      trace("pixel_check: cycle=%2u row=0x%x mask=0x%x last=%u cmd_id=%u\n",
            cycle, unsigned(out_bits->laddr.raw), unsigned(out_bits->mask),
            unsigned(out_bits->last), unsigned(out_bits->cmd_id));
    }
  }
}

void Driver::reset() {
  cycle_D_.reset(0);
  sent_D_.reset(0);
  received_D_.reset(0);
  in_val.reset(0);
  in_bits.reset(smesh::DmaReadResp{});
  out_rdy.reset(0);
  stalled_ = false;
  reused_ = false;
  skipped_while_stalled_ = false;
  completion_bytes_ = 0;
}

bool Driver::passed() const {
  unsigned expected_bytes = 0;
  for (const auto& row : test_.inputs) if (row.last == 1) expected_bytes += unsigned(row.bytes_read);
  return sent_Q_ == test_.inputs.size() && received_Q_ == test_.outputs.size() &&
         stalled_ && reused_ && (bypass_ || skipped_while_stalled_) && completion_bytes_ == expected_bytes;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver driver("Driver", bool(passthrough));
  smesh::MvinPixelRepeater repeater("MvinPixelRepeater", bool(passthrough));
  repeater.in_val << driver.in_val;
  repeater.in_bits << driver.in_bits;
  driver.in_rdy << repeater.in_rdy;
  driver.out_val << repeater.out_val;
  driver.out_bits << repeater.out_bits;
  repeater.out_rdy << driver.out_rdy;
  Clock clk;
  driver.clk << clk;
  repeater.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (unsigned cycle = 0; cycle < 3; ++cycle) Sim::run();
  // Reset while the first packet still has a stalled output.
  bool pass = true;
  for (unsigned run = 0; run < 2; ++run) {
    Sim::reset();
    for (unsigned cycle = 0; cycle < 100; ++cycle) Sim::run();
    pass &= driver.passed();
  }
  descore::flushLog();
  std::printf("[MVIN_PIXEL_REPEATER] %s dim=%zu passthrough=%u\n",
              pass ? "PASS" : "FAIL", smesh::kDim, unsigned(bool(passthrough)));
  return pass ? 0 : 1;
}
