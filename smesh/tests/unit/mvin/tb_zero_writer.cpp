// **********************************************************************
// smesh/tests/unit/mvin/tb_zero_writer.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 18 2026

// Check partial rows, block strides, held output, final-row completion accounting,
// and simultaneous last-row consumption/new-request acceptance on the zero branch.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "ZeroWriter.hpp"
#include "MvinPixelRepeater.hpp"
#include "SmeshCommand.hpp"

#include <algorithm>
#include <cstdio>

namespace {

smesh::DmaReadReq request(unsigned index) {
  smesh::DmaReadReq req{};
  req.all_zeros = 1;
  req.vaddr = 0x80001000; // The flag, not this address, selects local zero generation.
  req.laddr = index == 0 ? smesh::makeSpAddr(6) : smesh::makeAccAddr(2 + index);
  req.cols = u16(index == 0 ? 3 * smesh::kDim + 2 : index == 1 ? smesh::kDim + 2 : 1);
  req.block_stride = index == 0 ? 2 : 1;
  req.has_acc_bitwidth = bit(index == 1);
  req.scale = 0x7fc00000u; // NaN must not turn zero-fill into arithmetic scaling.
  req.repeats = 3;        // ZeroWriter does not perform ordinary row repetition.
  req.cmd_id = u16(7 + index);
  return req;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, req_val);
  Output(smesh::DmaReadReq, req_bits);
  Input(bit, req_rdy);
  Input(bit, resp_val);
  Input(smesh::DmaReadResp, resp_bits);
  Output(bit, resp_rdy);
  void updateInputs();
  void updateState();
  void reset();
  bool passed() const { return sent_Q_ == 3 && received_Q_ == 7 && stalled_ && reused_; }

 private:
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
  Output(u8, sent_Q_);
  Register(u8, sent_D_);
  Output(u8, received_Q_);
  Register(u8, received_D_);
  bool stalled_ = false;
  bool reused_ = false;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  sent_Q_ <= sent_D_;
  received_Q_ <= received_D_;
  UPDATE(updateInputs).reads(cycle_Q_, sent_Q_).writes(req_val, req_bits, resp_rdy);
  UPDATE(updateState).reads(cycle_Q_, sent_Q_, received_Q_, req_val, req_rdy,
                            resp_val, resp_bits, resp_rdy).writes(cycle_D_, sent_D_, received_D_);
}

void Driver::updateInputs() {
  const bool active = Sim::state != Sim::SimResetting;
  req_val = bit(active && sent_Q_ < 3);
  req_bits = sent_Q_ < 3 ? request(unsigned(*sent_Q_)) : smesh::DmaReadReq{};
  resp_rdy = bit(active && cycle_Q_ >= 5 && unsigned(*cycle_Q_) % 3 != 0);
}

void Driver::updateState() {
  if (Sim::state == Sim::SimResetting) return;
  cycle_D_ = u16(unsigned(*cycle_Q_) + 1);
  const bool push = req_val == 1 && req_rdy == 1;
  if (push) sent_D_ = u8(unsigned(*sent_Q_) + 1);
  if (resp_val == 0) return;
  const unsigned received = unsigned(*received_Q_);
  assert_always(received < 7, "ZeroWriter duplicated a row");
  const unsigned index = received < 4 ? 0 : received < 6 ? 1 : 2;
  const unsigned row_index = received < 4 ? received : received < 6 ? received - 4 : 0;
  const auto req = request(index);
  const unsigned count = std::min<unsigned>(smesh::kDim, unsigned(req.cols) - row_index * smesh::kDim);
  const bool last = (row_index + 1) * smesh::kDim >= unsigned(req.cols);
  const auto& row = *resp_bits;
  assert_always(row.laddr.raw == (req.laddr + row_index * unsigned(req.block_stride)).raw &&
                    row.mask == (1u << count) - 1 && row.last == bit(last) &&
                    row.cmd_id == req.cmd_id && row.len == req.cols &&
                    row.has_acc_bitwidth == req.has_acc_bitwidth && row.repeats == 0 &&
                    row.pixel_repeats == 1 && row.scale == smesh::kMvinScaleIdentityBits &&
                    row.bytes_read == unsigned(req.cols) *
                        (req.has_acc_bitwidth == 1 ? sizeof(smesh::Acc) : sizeof(smesh::Elem)),
                "ZeroWriter changed a held row, mask, address, or completion tag");
  for (auto byte : row.data) assert_always(byte == 0, "ZeroWriter produced nonzero data");
  if (resp_rdy == 0) stalled_ = true;
  if (!last || resp_rdy == 0)
    assert_always(req_rdy == 0, "ZeroWriter accepted a request before finishing its current request");
  if (resp_rdy == 1) {
    received_D_ = u8(received + 1);
    if (last && push) reused_ = true;
  }
}

void Driver::reset() {
  cycle_D_.reset(0);
  sent_D_.reset(0);
  received_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::DmaReadReq{});
  resp_rdy.reset(0);
  stalled_ = false;
  reused_ = false;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver driver("Driver");
  smesh::ZeroWriter writer("ZeroWriter");
  smesh::MvinPixelRepeater repeater("ZeroWriterPixelRepeater");
  writer.req_val << driver.req_val;
  writer.req_bits << driver.req_bits;
  driver.req_rdy << writer.req_rdy;
  repeater.in_val << writer.resp_val;
  repeater.in_bits << writer.resp_bits;
  writer.resp_rdy << repeater.in_rdy;
  driver.resp_val << repeater.out_val;
  driver.resp_bits << repeater.out_bits;
  repeater.out_rdy << driver.resp_rdy;
  Clock clk;
  driver.clk << clk;
  writer.clk << clk;
  repeater.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  bool pass = true;
  for (unsigned run = 0; run < 2; ++run) {
    Sim::reset();
    for (unsigned cycle = 0; cycle < 40; ++cycle) Sim::run();
    pass &= driver.passed();
  }
  std::printf("[ZERO_WRITER] %s\n", pass ? "PASS" : "FAIL");
  return pass ? 0 : 1;
}
