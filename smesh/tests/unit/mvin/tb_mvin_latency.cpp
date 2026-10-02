// **********************************************************************
// smesh/tests/unit/mvin/tb_mvin_latency.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 1 2026

// Check four-cycle MVIN row latency, throughput, and backpressure.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "MvinScale.hpp"
#include "SmeshCommand.hpp"

#include <cstdio>

namespace {

constexpr unsigned kInputRows = 7;
constexpr unsigned kOutputRows = kInputRows + 1;
constexpr unsigned kLatency = 4;

smesh::DmaReadResp makeRow(unsigned index) {
  smesh::DmaReadResp row{};
  row.cmd_id = u16(20 + index);
  row.laddr = smesh::makeSpAddr(3);
  row.repeats = u16(index == 1 ? 1 : 0);
  row.last = 1;
  row.scale = index == kInputRows - 1 ? smesh::kMvinScaleIdentityBits : 0x3f000000u;
  for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
    row.data[lane] = static_cast<std::uint8_t>(2 * (index + 1));
  }
  return row;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, in_val);
  Output(smesh::DmaReadResp, in_bits);
  Input(bit, in_rdy);
  Input(bit, out_val);
  Input(smesh::DmaReadResp, out_bits);
  Output(bit, out_rdy);

  void updateInput();
  void updateReady();
  void updateState();
  void reset();
  bool passed() const;

 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  Output(u8, sent_Q_);
  Register(u8, sent_D_);
  Output(u8, received_Q_);
  Register(u8, received_D_);
  Output(u8, first_accept_cycle_Q_);
  Register(u8, first_accept_cycle_D_);
  Output(bit, saw_stall_Q_);
  Register(bit, saw_stall_D_);
  Output(bit, saw_full_Q_);
  Register(bit, saw_full_D_);
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  sent_Q_ <= sent_D_;
  received_Q_ <= received_D_;
  first_accept_cycle_Q_ <= first_accept_cycle_D_;
  saw_stall_Q_ <= saw_stall_D_;
  saw_full_Q_ <= saw_full_D_;
  UPDATE(updateInput).reads(sent_Q_).writes(in_val, in_bits);
  UPDATE(updateReady).reads(cycle_Q_).writes(out_rdy);
  UPDATE(updateState)
      .reads(cycle_Q_, sent_Q_, received_Q_, first_accept_cycle_Q_,
             in_val, in_rdy, out_val, out_bits)
      .reads(out_rdy)
      .writes(cycle_D_, sent_D_, received_D_, first_accept_cycle_D_,
              saw_stall_D_, saw_full_D_);
}

void Driver::updateInput() {
  const auto sent = static_cast<unsigned>(static_cast<std::uint8_t>(*sent_Q_));
  const bool active = Sim::state != Sim::SimResetting && sent < kInputRows;
  in_val = bit(active);
  in_bits = active ? makeRow(sent) : smesh::DmaReadResp{};
}

void Driver::updateReady() {
  const auto cycle = static_cast<unsigned>(static_cast<std::uint8_t>(*cycle_Q_));
  out_rdy = bit(Sim::state != Sim::SimResetting && (cycle < 6 || cycle >= 11));
}

void Driver::updateState() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(static_cast<std::uint8_t>(*cycle_Q_));
  const auto sent = static_cast<unsigned>(static_cast<std::uint8_t>(*sent_Q_));
  const auto received = static_cast<unsigned>(static_cast<std::uint8_t>(*received_Q_));
  cycle_D_ = u8(cycle + 1);

  if (in_val == 1 && in_rdy == 1) {
    if (sent == 0) first_accept_cycle_D_ = u8(cycle);
    sent_D_ = u8(sent + 1);
  } else if (in_val == 1) {
    saw_full_D_ = 1;
  }

  if (sent > 0) {
    const auto first = static_cast<unsigned>(static_cast<std::uint8_t>(*first_accept_cycle_Q_));
    if (cycle < first + kLatency) {
      assert_always(out_val == 0, "MVIN row appeared before configured latency");
    }
    if (cycle == first + kLatency || cycle == first + kLatency + 1) {
      assert_always(out_val == 1, "MVIN rows did not emerge on successive cycles");
    }
  }

  if (out_val == 1) {
    assert_always(received < kOutputRows, "MVIN emitted too many rows");
    const unsigned input_index = received <= 2 ? (received == 0 ? 0 : 1) : received - 1;
    const auto row = *out_bits;
    const unsigned expected_data = input_index == kInputRows - 1
                                       ? 2 * (input_index + 1) : input_index + 1;
    assert_always(row.cmd_id == 20 + input_index &&
                      row.laddr.data() == 3 + (received == 1 ? 1u : 0u) &&
                      row.last == bit(received != 1) &&
                      row.data[0] == expected_data,
                  "MVIN delayed, repeated, or scaled a row incorrectly");
    if (out_rdy == 1) {
      received_D_ = u8(received + 1);
    } else {
      saw_stall_D_ = 1;
    }
  }
}

void Driver::reset() {
  cycle_D_.reset(0);
  sent_D_.reset(0);
  received_D_.reset(0);
  first_accept_cycle_D_.reset(0);
  saw_stall_D_.reset(0);
  saw_full_D_.reset(0);
  in_val.reset(0);
  in_bits.reset(smesh::DmaReadResp{});
  out_rdy.reset(0);
}

bool Driver::passed() const {
  return sent_Q_ == kInputRows && received_Q_ == kOutputRows &&
         saw_stall_Q_ == 1 && saw_full_Q_ == 1;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  Driver driver("Driver");
  smesh::MvinScale scale("Scale", kLatency);
  scale.in_val << driver.in_val;
  scale.in_bits << driver.in_bits;
  driver.in_rdy << scale.in_rdy;
  driver.out_val << scale.out_val;
  driver.out_bits << scale.out_bits;
  scale.out_rdy << driver.out_rdy;

  Clock clk;
  driver.clk << clk;
  scale.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 32; ++cycle) Sim::run();

  const bool ok = driver.passed();
  std::printf("[MVIN_LATENCY] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
