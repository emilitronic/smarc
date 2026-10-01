// **********************************************************************
// smesh/tests/unit/mvin/tb_mvin_handshake.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 30 2026

// Check held outputs and simultaneous input/output handshakes in the load path.
// After adding explicity val/rdy signals to MvinScaleSplit, MvinScale, MvinScaleAcc,
// and MvinLocalRouter, the load path should be able to hold outputs until the next
// input arrives, and should be able to accept new inputs while holding outputs.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "MvinLocalRouter.hpp"
#include "MvinScale.hpp"

#include <cstdio>

namespace {

smesh::DmaReadResp makeRow(unsigned id, bool acc) {
  smesh::DmaReadResp row{};
  row.cmd_id = u16(id);
  row.laddr = acc ? smesh::makeAccAddr(id) : smesh::makeSpAddr(id);
  row.has_acc_bitwidth = bit(acc);
  row.data[0] = static_cast<std::uint8_t>(id);
  row.mask = 1;
  row.len = 1;
  row.last = 1;
  return row;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, scale_in_val);
  Output(smesh::DmaReadResp, scale_in_bits);
  Input(bit, scale_in_rdy);
  Input(bit, scale_out_val);
  Input(smesh::DmaReadResp, scale_out_bits);
  Output(bit, scale_out_rdy);

  Output(bit, router_in_val);
  Output(smesh::DmaReadResp, router_in_bits);
  Input(bit, router_in_rdy);
  Input(bit, spad_val);
  Input(smesh::DmaReadResp, spad_bits);
  Output(bit, spad_rdy);
  Input(bit, accum_val);
  Input(smesh::DmaReadResp, accum_bits);
  Output(bit, accum_rdy);

  void updateInputs();
  void updateReady();
  void updateState();
  void reset();
  bool passed() const;

 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  Output(u8, scale_sent_Q_);
  Register(u8, scale_sent_D_);
  Output(u8, scale_received_Q_);
  Register(u8, scale_received_D_);
  Output(u8, router_sent_Q_);
  Register(u8, router_sent_D_);
  Output(u8, router_received_Q_);
  Register(u8, router_received_D_);
  Output(bit, scale_stalled_Q_);
  Register(bit, scale_stalled_D_);
  Output(bit, router_stalled_Q_);
  Register(bit, router_stalled_D_);
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  scale_sent_Q_ <= scale_sent_D_;
  scale_received_Q_ <= scale_received_D_;
  router_sent_Q_ <= router_sent_D_;
  router_received_Q_ <= router_received_D_;
  scale_stalled_Q_ <= scale_stalled_D_;
  router_stalled_Q_ <= router_stalled_D_;
  UPDATE(updateInputs).reads(scale_sent_Q_, router_sent_Q_)
      .writes(scale_in_val, scale_in_bits, router_in_val, router_in_bits);
  UPDATE(updateReady).reads(cycle_Q_)
      .writes(scale_out_rdy, spad_rdy, accum_rdy);
  UPDATE(updateState)
      .reads(cycle_Q_, scale_sent_Q_, scale_received_Q_, router_sent_Q_,
             router_received_Q_, scale_in_val, scale_in_rdy, scale_out_val)
      .reads(scale_out_bits, scale_out_rdy, router_in_val, router_in_rdy,
             spad_val, spad_bits, spad_rdy, accum_val)
      .reads(accum_bits, accum_rdy)
      .writes(cycle_D_, scale_sent_D_, scale_received_D_, router_sent_D_,
              router_received_D_, scale_stalled_D_, router_stalled_D_);
}

void Driver::updateInputs() {
  const auto scale_index = static_cast<unsigned>(static_cast<std::uint8_t>(*scale_sent_Q_));
  const auto router_index = static_cast<unsigned>(static_cast<std::uint8_t>(*router_sent_Q_));
  const bool active = Sim::state != Sim::SimResetting;
  scale_in_val = bit(active && scale_index < 2);
  scale_in_bits = active && scale_index < 2 ? makeRow(10 + scale_index, true)
                                             : smesh::DmaReadResp{};
  router_in_val = bit(active && router_index < 2);
  router_in_bits = active && router_index < 2 ? makeRow(20 + router_index, router_index == 1)
                                               : smesh::DmaReadResp{};
}

void Driver::updateReady() {
  const bool ready = Sim::state != Sim::SimResetting && cycle_Q_ >= 5;
  scale_out_rdy = bit(ready);
  spad_rdy = bit(ready);
  accum_rdy = bit(ready);
}

void Driver::updateState() {
  if (Sim::state == Sim::SimResetting) return;
  cycle_D_ = u8(static_cast<std::uint8_t>(*cycle_Q_) + 1);
  if (scale_in_val == 1 && scale_in_rdy == 1) {
    scale_sent_D_ = u8(static_cast<std::uint8_t>(*scale_sent_Q_) + 1);
  }
  if (router_in_val == 1 && router_in_rdy == 1) {
    router_sent_D_ = u8(static_cast<std::uint8_t>(*router_sent_Q_) + 1);
  }

  if (scale_out_val == 1) {
    const auto expected = 10 + static_cast<unsigned>(static_cast<std::uint8_t>(*scale_received_Q_));
    assert_always(static_cast<unsigned>(scale_out_bits->cmd_id) == expected &&
                      scale_out_bits->data[0] == expected,
                  "MvinScaleAcc changed or reordered a held row");
    if (scale_out_rdy == 1) {
      scale_received_D_ = u8(static_cast<std::uint8_t>(*scale_received_Q_) + 1);
    } else {
      scale_stalled_D_ = 1;
    }
  }

  assert_always(!(spad_val == 1 && accum_val == 1),
                "MvinLocalRouter presented one row to both memories");
  if (spad_val == 1 || accum_val == 1) {
    const auto expected = 20 + static_cast<unsigned>(static_cast<std::uint8_t>(*router_received_Q_));
    const auto& row = spad_val == 1 ? *spad_bits : *accum_bits;
    const bool route_ok = (expected == 20 && spad_val == 1) ||
                          (expected == 21 && accum_val == 1);
    assert_always(route_ok && static_cast<unsigned>(row.cmd_id) == expected &&
                      row.data[0] == expected,
                  "MvinLocalRouter changed, misrouted, or reordered a held row");
    const bool accepted = spad_val == 1 ? spad_rdy == 1 : accum_rdy == 1;
    if (accepted) {
      router_received_D_ = u8(static_cast<std::uint8_t>(*router_received_Q_) + 1);
    } else {
      router_stalled_D_ = 1;
    }
  }
}

void Driver::reset() {
  cycle_D_.reset(0);
  scale_sent_D_.reset(0);
  scale_received_D_.reset(0);
  router_sent_D_.reset(0);
  router_received_D_.reset(0);
  scale_stalled_D_.reset(0);
  router_stalled_D_.reset(0);
  scale_in_val.reset(0);
  scale_in_bits.reset(smesh::DmaReadResp{});
  router_in_val.reset(0);
  router_in_bits.reset(smesh::DmaReadResp{});
  scale_out_rdy.reset(0);
  spad_rdy.reset(0);
  accum_rdy.reset(0);
}

bool Driver::passed() const {
  return scale_sent_Q_ == 2 && scale_received_Q_ == 2 &&
         router_sent_Q_ == 2 && router_received_Q_ == 2 &&
         scale_stalled_Q_ == 1 && router_stalled_Q_ == 1;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  Driver driver("Driver");
  smesh::MvinScaleAcc scale_acc("ScaleAcc");
  smesh::MvinLocalRouter router("Router");

  scale_acc.in_val << driver.scale_in_val;
  scale_acc.in_bits << driver.scale_in_bits;
  driver.scale_in_rdy << scale_acc.in_rdy;
  driver.scale_out_val << scale_acc.out_val;
  driver.scale_out_bits << scale_acc.out_bits;
  scale_acc.out_rdy << driver.scale_out_rdy;

  router.in_val << driver.router_in_val;
  router.in_bits << driver.router_in_bits;
  driver.router_in_rdy << router.in_rdy;
  driver.spad_val << router.dmaread_spad_val;
  driver.spad_bits << router.dmaread_spad_bits;
  router.dmaread_spad_rdy << driver.spad_rdy;
  driver.accum_val << router.dmaread_accum_val;
  driver.accum_bits << router.dmaread_accum_bits;
  router.dmaread_accum_rdy << driver.accum_rdy;

  Clock clk;
  driver.clk << clk;
  scale_acc.clk << clk;
  router.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 12; ++cycle) Sim::run();

  const bool ok = driver.passed();
  std::printf("[MVIN_HANDSHAKE] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
