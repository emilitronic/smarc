// **********************************************************************
// smesh/tests/unit/mvin/tb_mvin_repeat.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 30 2026

// Check repeated local rows, final completion, and output backpressure.
// Check that identity scale rows bypass scale logic.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "MvinScale.hpp"
#include "SmeshCommand.hpp"

#include <cstdio>

namespace {

smesh::DmaReadResp makeRow(unsigned id, bool accum) {
  smesh::DmaReadResp row{};
  row.cmd_id = u16(id);
  row.laddr = accum ? smesh::makeAccAddr(6, true) : smesh::makeSpAddr(4);
  if (accum) {
    row.data[0] = static_cast<std::uint8_t>(id);
  } else {
    const int values[] = {-3, -1, 1, 3};
    const int clipped[] = {100, -100, 127, -128};
    const int identity[] = {-128, -1, 0, 127};
    for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
      row.data[lane] = static_cast<std::uint8_t>(id == 10 ? values[lane % 4]
                                             : id == 11 ? clipped[lane % 4]
                                                        : identity[lane % 4]);
    }
    row.scale = id == 10 ? 0x3f000000u : id == 11 ? 0x40000000u
                                                  : smesh::kMvinScaleIdentityBits;
  }
  row.mask = 1;
  row.len = 1;
  row.bytes_read = u16(accum ? sizeof(smesh::Acc) : sizeof(smesh::Elem));
  row.repeats = u16(id == 10 ? 2 : 0);
  row.last = 1;
  return row;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, normal_in_val);
  Output(smesh::DmaReadResp, normal_in_bits);
  Input(bit, normal_in_rdy);
  Input(bit, normal_out_val);
  Input(smesh::DmaReadResp, normal_out_bits);
  Output(bit, normal_out_rdy);
  Output(bit, accum_in_val);
  Output(smesh::DmaReadResp, accum_in_bits);
  Input(bit, accum_in_rdy);
  Input(bit, accum_out_val);
  Input(smesh::DmaReadResp, accum_out_bits);
  Output(bit, accum_out_rdy);

  void updateInputs();
  void updateReady();
  void updateState();
  void reset();
  bool passed() const;

 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  Output(u8, normal_sent_Q_);
  Register(u8, normal_sent_D_);
  Output(u8, accum_sent_Q_);
  Register(u8, accum_sent_D_);
  Output(u8, normal_received_Q_);
  Register(u8, normal_received_D_);
  Output(u8, accum_received_Q_);
  Register(u8, accum_received_D_);
  Output(bit, normal_stalled_Q_);
  Register(bit, normal_stalled_D_);
  Output(bit, accum_stalled_Q_);
  Register(bit, accum_stalled_D_);
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  normal_sent_Q_ <= normal_sent_D_;
  accum_sent_Q_ <= accum_sent_D_;
  normal_received_Q_ <= normal_received_D_;
  accum_received_Q_ <= accum_received_D_;
  normal_stalled_Q_ <= normal_stalled_D_;
  accum_stalled_Q_ <= accum_stalled_D_;
  UPDATE(updateInputs).reads(normal_sent_Q_, accum_sent_Q_)
      .writes(normal_in_val, normal_in_bits, accum_in_val, accum_in_bits);
  UPDATE(updateReady).reads(cycle_Q_).writes(normal_out_rdy, accum_out_rdy);
  UPDATE(updateState)
      .reads(cycle_Q_, normal_sent_Q_, accum_sent_Q_, normal_received_Q_,
             accum_received_Q_, normal_in_val, normal_in_rdy, normal_out_val)
      .reads(normal_out_bits, normal_out_rdy, accum_in_val, accum_in_rdy,
             accum_out_val, accum_out_bits, accum_out_rdy)
      .writes(cycle_D_, normal_sent_D_, accum_sent_D_, normal_received_D_,
              accum_received_D_, normal_stalled_D_, accum_stalled_D_);
}

void Driver::updateInputs() {
  const auto normal_sent = static_cast<std::uint8_t>(*normal_sent_Q_);
  const auto accum_sent = static_cast<std::uint8_t>(*accum_sent_Q_);
  const bool active = Sim::state != Sim::SimResetting;
  normal_in_val = bit(active && normal_sent < 3);
  normal_in_bits = active && normal_sent < 3 ? makeRow(10 + normal_sent, false)
                                                     : smesh::DmaReadResp{};
  accum_in_val = bit(active && accum_sent < 2);
  accum_in_bits = active && accum_sent < 2 ? makeRow(10 + accum_sent, true)
                                                   : smesh::DmaReadResp{};
}

void Driver::updateReady() {
  const auto cycle = static_cast<std::uint8_t>(*cycle_Q_);
  const bool ready = Sim::state != Sim::SimResetting && cycle >= 4 && cycle != 6;
  normal_out_rdy = bit(ready);
  accum_out_rdy = bit(ready);
}

void Driver::updateState() {
  if (Sim::state == Sim::SimResetting) return;
  cycle_D_ = u8(static_cast<std::uint8_t>(*cycle_Q_) + 1);
  if (normal_in_val == 1 && normal_in_rdy == 1) {
    normal_sent_D_ = u8(static_cast<std::uint8_t>(*normal_sent_Q_) + 1);
  }
  if (accum_in_val == 1 && accum_in_rdy == 1) {
    accum_sent_D_ = u8(static_cast<std::uint8_t>(*accum_sent_Q_) + 1);
  }

  const auto check = [this](smesh::DmaReadResp row, unsigned index, bool accum) {
    const unsigned offset = index < 3 ? 2 - index : 0;
    const unsigned id = index < 3 ? 10 : index == 3 ? 11 : 12;
    const unsigned base = accum ? 6 : 4;
    assert_always(index < (accum ? 4u : 5u) && row.laddr.data() == base + offset &&
                      row.laddr.is_acc_addr() == accum &&
                      row.laddr.accumulate() == accum &&
                      row.cmd_id == id &&
                      row.mask == 1 && row.len == 1 &&
                      row.bytes_read == (accum ? sizeof(smesh::Acc) : sizeof(smesh::Elem)) &&
                      row.last == bit(index >= 2),
                  "MvinScale emitted the wrong repeated row");
    if (accum) {
      assert_always(row.data[0] == id, "MvinScaleAcc changed a full-width row");
    } else {
      const int half[] = {-2, 0, 0, 2};
      const int clipped[] = {127, -128, 127, -128};
      const int identity[] = {-128, -1, 0, 127};
      for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
        const auto expected = index < 3 ? half[lane % 4]
                             : index == 3 ? clipped[lane % 4]
                                          : identity[lane % 4];
        assert_always(row.data[lane] == static_cast<std::uint8_t>(expected),
                      "MvinScale rounded or clipped a lane incorrectly");
      }
    }
  };

  if (normal_out_val == 1) {
    const auto index = static_cast<std::uint8_t>(*normal_received_Q_);
    check(*normal_out_bits, index, false);
    if (index < 2) assert_always(normal_in_rdy == 0, "MvinScale replaced a repeated row");
    if (normal_out_rdy == 1) {
      normal_received_D_ = u8(index + 1);
    } else {
      normal_stalled_D_ = 1;
    }
  }
  if (accum_out_val == 1) {
    const auto index = static_cast<std::uint8_t>(*accum_received_Q_);
    check(*accum_out_bits, index, true);
    if (index < 2) assert_always(accum_in_rdy == 0, "MvinScaleAcc replaced a repeated row");
    if (accum_out_rdy == 1) {
      accum_received_D_ = u8(index + 1);
    } else {
      accum_stalled_D_ = 1;
    }
  }
}

void Driver::reset() {
  cycle_D_.reset(0);
  normal_sent_D_.reset(0);
  accum_sent_D_.reset(0);
  normal_received_D_.reset(0);
  accum_received_D_.reset(0);
  normal_stalled_D_.reset(0);
  accum_stalled_D_.reset(0);
  normal_in_val.reset(0);
  normal_in_bits.reset(smesh::DmaReadResp{});
  accum_in_val.reset(0);
  accum_in_bits.reset(smesh::DmaReadResp{});
  normal_out_rdy.reset(0);
  accum_out_rdy.reset(0);
}

bool Driver::passed() const {
  return normal_sent_Q_ == 3 && accum_sent_Q_ == 2 &&
         normal_received_Q_ == 5 && accum_received_Q_ == 4 &&
         normal_stalled_Q_ == 1 && accum_stalled_Q_ == 1;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  Driver driver("Driver");
  smesh::MvinScale normal("Normal");
  smesh::MvinScaleAcc accum("Accum");

  normal.in_val << driver.normal_in_val;
  normal.in_bits << driver.normal_in_bits;
  driver.normal_in_rdy << normal.in_rdy;
  driver.normal_out_val << normal.out_val;
  driver.normal_out_bits << normal.out_bits;
  normal.out_rdy << driver.normal_out_rdy;

  accum.in_val << driver.accum_in_val;
  accum.in_bits << driver.accum_in_bits;
  driver.accum_in_rdy << accum.in_rdy;
  driver.accum_out_val << accum.out_val;
  driver.accum_out_bits << accum.out_bits;
  accum.out_rdy << driver.accum_out_rdy;

  Clock clk;
  driver.clk << clk;
  normal.clk << clk;
  accum.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 18; ++cycle) Sim::run();

  const bool ok = driver.passed();
  std::printf("[MVIN_REPEAT] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
