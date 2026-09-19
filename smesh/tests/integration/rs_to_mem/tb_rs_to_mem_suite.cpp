// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_suite.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 19 2026
/*
RS-to-memory integration runner for the ExCtrl slice of the rs_to_mem
domain. See README.md for scope.

Composition:
  cmd_valid/cmd_bits -> SmeshCmdQueue -> SmeshUnrolledCmdQueue -> SmeshRS
    -> ExCtrl -> (legacy-struct adapters) -> real Spad/Accum read+write
    arbiters -> real Spad/Accum.

ExCtrl is treated as an opaque, already-validated block (see ex_ctrl's own
integration suite for that). What's under test here is everything around
it: the real command-queue front end, the real SmeshRS scheduler, and the
real Spad/Accum plus their arbiters -- none of which ex_ctrl's own suite
exercises (it uses a synthetic RS-like driver and a fixed-latency
scratchpad stand-in instead).

Accumulator reads are never exercised by the basic/mul_pre programs (every
read operand comes from spad; accum is only ever a PRELOAD destination), so
the accumulator read-response pipeline (ArbReadAccum, StNormCtrl,
Normalizer, AccScaleUnit, AccumExResp) is intentionally not instantiated.

Run:
  cmake --build build --target tb_rs_to_mem_suite -j
  ./build/smesh/tb_rs_to_mem_suite -test=basic
  ./build/smesh/tb_rs_to_mem_suite -test=mul_pre
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ArbReadLocal.hpp"
#include "ArbWriteLocal.hpp"
#include "Accum.hpp"
#include "ExCtrl.hpp"
#include "SmeshCmdQueues.hpp"
#include "SmeshCommand.hpp"
#include "SmeshRS.hpp"
#include "Spad.hpp"
#include "SpadReadPipes.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace smesh;

// ********************************************************
// Test case data: raw SmeshCmd programs, initial spad
// contents, and the expected accumulator result(s).
// ********************************************************

struct SpadPreloadRow {
  SmeshLocalAddr laddr;
  MeshInputRow data;
};

struct ExpectedAccumResult {
  SmeshLocalAddr base;
  std::vector<MeshAccumRow> rows;
};

struct RsMemTestCase {
  std::string name;
  std::string description;
  std::vector<SmeshCmd> program;
  std::vector<SpadPreloadRow> spad_rows;
  std::vector<ExpectedAccumResult> expected_results;
  std::vector<SmeshRsTag> expected_completion_order;
  int max_cycles = 200;
  int drain_cycles = 8;
};

std::vector<SpadPreloadRow> spadRows(std::uint32_t base,
                                     std::initializer_list<MeshInputRow> rows) {
  std::vector<SpadPreloadRow> out;
  std::uint32_t row = base;
  for (const auto& data : rows) {
    out.push_back(SpadPreloadRow{makeSpAddr(row), data});
    ++row;
  }
  return out;
}

MeshInputRow row(std::initializer_list<int> values) {
  MeshInputRow r{};
  std::size_t lane = 0;
  for (const int v : values) {
    r[lane++] = static_cast<Elem>(v);
  }
  return r;
}

// C = A*B + D, one output row at a time.
MeshAccumRow matmulRow(const std::array<MeshInputRow, kDim>& a,
                       const std::array<MeshInputRow, kDim>& b,
                       const std::array<MeshInputRow, kDim>& d,
                       std::size_t r) {
  MeshAccumRow result{};
  for (std::size_t col = 0; col < kDim; ++col) {
    Acc value = static_cast<Acc>(d[r][col]);
    for (std::size_t k = 0; k < kDim; ++k) {
      value += static_cast<Acc>(a[r][k]) * static_cast<Acc>(b[k][col]);
    }
    result[col] = value;
  }
  return result;
}

std::array<MeshInputRow, kDim> toArray(const std::vector<MeshInputRow>& rows) {
  std::array<MeshInputRow, kDim> out{};
  for (std::size_t i = 0; i < kDim && i < rows.size(); ++i) {
    out[i] = rows[i];
  }
  return out;
}

SmeshCmd configExCmd() {
  SmeshCmd cmd{};
  cmd.funct = u32(static_cast<std::uint32_t>(SmeshFunct::Config));
  cmd.rs1 = u64(packConfigExRs1(/*a_stride=*/1));
  cmd.rs2 = u64(packConfigExRs2(/*c_stride=*/1));
  return cmd;
}

SmeshCmd preloadCmd(SmeshLocalAddr weights, SmeshLocalAddr dest) {
  SmeshCmd cmd{};
  cmd.funct = u32(static_cast<std::uint32_t>(SmeshFunct::Preload));
  cmd.rs1 = u64(packLocal(weights, MatrixShape{kDim, kDim}));
  cmd.rs2 = u64(packLocal(dest, MatrixShape{kDim, kDim}));
  return cmd;
}

SmeshCmd computeFlipCmd(SmeshLocalAddr input, SmeshLocalAddr addend) {
  SmeshCmd cmd{};
  cmd.funct = u32(static_cast<std::uint32_t>(SmeshFunct::ComputeFlip));
  cmd.rs1 = u64(packLocal(input, MatrixShape{kDim, kDim}));
  cmd.rs2 = u64(packLocal(addend, MatrixShape{kDim, kDim}));
  return cmd;
}

SmeshCmd computeStayCmd(SmeshLocalAddr input, SmeshLocalAddr addend) {
  SmeshCmd cmd{};
  cmd.funct = u32(static_cast<std::uint32_t>(SmeshFunct::ComputeStay));
  cmd.rs1 = u64(packLocal(input, MatrixShape{kDim, kDim}));
  cmd.rs2 = u64(packLocal(addend, MatrixShape{kDim, kDim}));
  return cmd;
}

// "basic": CONFIG, PRELOAD(B0,C0), COMPUTE_FLIP(A0,D0), COMPUTE_STAY(A1,D1).
// Mirrors ex_ctrl's makeBasicFlipStayTest(), but issued as raw SmeshCmds
// through the real cmd-queue+RS path instead of hand-tagged SmeshIssues.
RsMemTestCase makeBasicCase() {
  RsMemTestCase test{};
  test.name = "basic";
  test.description = "CONFIG, PRELOAD, COMPUTE_FLIP, COMPUTE_STAY via real RS";

  const auto d1 = makeSpAddr(0);   // bank 0
  const auto b0 = makeSpAddr(4);   // bank 1 (doubles as math D for COMPUTE_FLIP)
  const auto a1 = makeSpAddr(8);   // bank 2
  const auto a0 = makeSpAddr(12);  // bank 3
  const auto c0 = makeAccAddr(8);  // accum bank 1

  const std::vector<MeshInputRow> d1_rows = {
      row({0, 1, 2, 3}), row({4, 5, 6, 7}), row({8, 9, 10, 11}), row({12, 13, 14, 15})};
  const std::vector<MeshInputRow> b0_rows = {
      row({16, 17, 18, 19}), row({20, 21, 22, 23}), row({24, 25, 26, 27}), row({28, 29, 30, 31})};
  const std::vector<MeshInputRow> a1_rows = {
      row({32, 33, 34, 35}), row({36, 37, 38, 39}), row({40, 41, 42, 43}), row({44, 45, 46, 47})};
  const std::vector<MeshInputRow> a0_rows = {
      row({48, 49, 50, 51}), row({52, 53, 54, 55}), row({56, 57, 58, 59}), row({60, 61, 62, 63})};

  test.spad_rows = spadRows(0, {d1_rows[0], d1_rows[1], d1_rows[2], d1_rows[3]});
  const auto b0v = spadRows(4, {b0_rows[0], b0_rows[1], b0_rows[2], b0_rows[3]});
  const auto a1v = spadRows(8, {a1_rows[0], a1_rows[1], a1_rows[2], a1_rows[3]});
  const auto a0v = spadRows(12, {a0_rows[0], a0_rows[1], a0_rows[2], a0_rows[3]});
  test.spad_rows.insert(test.spad_rows.end(), b0v.begin(), b0v.end());
  test.spad_rows.insert(test.spad_rows.end(), a1v.begin(), a1v.end());
  test.spad_rows.insert(test.spad_rows.end(), a0v.begin(), a0v.end());

  test.program = {
      configExCmd(),
      preloadCmd(b0, c0),
      computeFlipCmd(a0, b0),  // D0 == B0's location
      computeStayCmd(a1, d1),
  };

  // Sequential allocation order => rs_tag 0,1,2,3 for CONFIG,PRELOAD,FLIP,STAY.
  // COMPUTE_STAY has no following PRELOAD, so its result is never written,
  // matching ex_ctrl's own basic scenario.
  test.expected_completion_order = {0, 2, 3, 1};

  const auto a0arr = toArray(a0_rows);
  const auto b0arr = toArray(b0_rows);
  const auto d0arr = toArray(b0_rows);  // D0 reused B0's location
  std::vector<MeshAccumRow> c0_rows;
  for (std::size_t r = 0; r < kDim; ++r) {
    c0_rows.push_back(matmulRow(a0arr, b0arr, d0arr, r));
  }
  test.expected_results = {ExpectedAccumResult{c0, c0_rows}};
  return test;
}

// "mul_pre": CONFIG, PRELOAD(B0,C0), COMPUTE_FLIP(A0,D0)+PRELOAD(B1,C1)
// overlap, COMPUTE_FLIP(A1,D1). Mirrors ex_ctrl's makeComputePreloadOverlapTest().
RsMemTestCase makeMulPreCase() {
  RsMemTestCase test{};
  test.name = "mul_pre";
  test.description = "COMPUTE+PRELOAD overlap, two results, via real RS";

  const auto a0 = makeSpAddr(0);   // bank 0 (A0 == A1)
  const auto d0 = makeSpAddr(4);   // bank 1 (D0 == D1)
  const auto b1 = makeSpAddr(8);   // bank 2
  const auto b0 = makeSpAddr(12);  // bank 3
  const auto c0 = makeAccAddr(0);  // accum bank 0
  const auto c1 = makeAccAddr(8);  // accum bank 1

  const std::vector<MeshInputRow> a0_rows = {
      row({0, 1, 2, 3}), row({4, 5, 6, 7}), row({8, 9, 10, 11}), row({12, 13, 14, 15})};
  const std::vector<MeshInputRow> d0_rows = {
      row({16, 17, 18, 19}), row({20, 21, 22, 23}), row({24, 25, 26, 27}), row({28, 29, 30, 31})};
  const std::vector<MeshInputRow> b1_rows = {
      row({32, 33, 34, 35}), row({36, 37, 38, 39}), row({40, 41, 42, 43}), row({44, 45, 46, 47})};
  const std::vector<MeshInputRow> b0_rows = {
      row({48, 49, 50, 51}), row({52, 53, 54, 55}), row({56, 57, 58, 59}), row({60, 61, 62, 63})};

  test.spad_rows = spadRows(0, {a0_rows[0], a0_rows[1], a0_rows[2], a0_rows[3]});
  const auto d0v = spadRows(4, {d0_rows[0], d0_rows[1], d0_rows[2], d0_rows[3]});
  const auto b1v = spadRows(8, {b1_rows[0], b1_rows[1], b1_rows[2], b1_rows[3]});
  const auto b0v = spadRows(12, {b0_rows[0], b0_rows[1], b0_rows[2], b0_rows[3]});
  test.spad_rows.insert(test.spad_rows.end(), d0v.begin(), d0v.end());
  test.spad_rows.insert(test.spad_rows.end(), b1v.begin(), b1v.end());
  test.spad_rows.insert(test.spad_rows.end(), b0v.begin(), b0v.end());

  test.program = {
      configExCmd(),
      preloadCmd(b0, c0),
      computeFlipCmd(a0, d0),
      preloadCmd(b1, c1),
      computeFlipCmd(a0, d0),  // A1==A0, D1==D0
  };

  // Sequential rs_tag 0..4 for CONFIG,PRELOAD0,FLIP0,PRELOAD1,FLIP1.
  test.expected_completion_order = {0, 2, 4, 1, 3};

  const auto aarr = toArray(a0_rows);
  const auto darr = toArray(d0_rows);
  const auto b0arr = toArray(b0_rows);
  const auto b1arr = toArray(b1_rows);
  std::vector<MeshAccumRow> c0_rows;
  std::vector<MeshAccumRow> c1_rows;
  for (std::size_t r = 0; r < kDim; ++r) {
    c0_rows.push_back(matmulRow(aarr, b0arr, darr, r));
    c1_rows.push_back(matmulRow(aarr, b1arr, darr, r));
  }
  test.expected_results = {ExpectedAccumResult{c0, c0_rows},
                           ExpectedAccumResult{c1, c1_rows}};
  return test;
}

std::vector<RsMemTestCase> rsMemTestCases() {
  return {makeBasicCase(), makeMulPreCase()};
}

// ********************************************************
// Test-only glue components.
// ********************************************************

// Issues one raw SmeshCmd per cycle into cmd_valid/cmd_bits, holding until
// cmd_ready. Waits for spad preload to finish first so ExCtrl never reads
// stale/uninitialized scratchpad data.
class RsMemCmdDriver : public Component {
  DECLARE_COMPONENT(RsMemCmdDriver);

 public:
  RsMemCmdDriver(const std::vector<SmeshCmd>& program, std::string name, COMPONENT_CTOR)
      : program_(program) {
    UPDATE(update).reads(cmd_ready, preload_done).writes(cmd_valid, cmd_bits);
  }

  Clock(clk);
  Output(bit, cmd_valid);
  Output(SmeshCmd, cmd_bits);
  Input(bit, cmd_ready);
  Input(bit, preload_done);

  bool done() const { return next_ >= program_.size(); }

  void update() {
    cmd_valid = 0;
    cmd_bits = SmeshCmd{};
    if (Sim::state == Sim::SimResetting) {
      return;
    }
    if (preload_done == 0 || done()) {
      return;
    }
    cmd_bits = program_[next_];
    cmd_valid = 1;
    if (cmd_ready != 0) {
      ++next_;
    }
  }

  void reset() { next_ = 0; }

 private:
  const std::vector<SmeshCmd>& program_;
  std::size_t next_ = 0;
};

// Sequentially writes the test's initial spad image into the real Spad,
// one row (one bank) per cycle, via the same dmaread-shaped write channel
// WriteCtrl would normally drive. Deliberately one write at a time so
// preload itself never exercises Spad's known single-bank-per-cycle limit.
class SpadPreloadDriver : public Component {
  DECLARE_COMPONENT(SpadPreloadDriver);

 public:
  SpadPreloadDriver(const std::vector<SpadPreloadRow>& rows, std::string name, COMPONENT_CTOR)
      : rows_(rows) {
    UPDATE(update).reads(dmaread_rdy).writes(dmaread_val, dmaread_bits, done);
  }

  Clock(clk);
  OutputArray(bit, dmaread_val, kSpBanks);
  OutputArray(DmaReadResp, dmaread_bits, kSpBanks);
  InputArray(bit, dmaread_rdy, kSpBanks);
  Output(bit, done);

  void update() {
    for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
      dmaread_val[bank] = 0;
      dmaread_bits[bank] = DmaReadResp{};
    }
    if (next_ >= rows_.size()) {
      done = 1;
      return;
    }
    done = 0;

    const auto& entry = rows_[next_];
    const auto bank = entry.laddr.sp_bank();
    DmaReadResp resp{};
    resp.laddr = entry.laddr;
    for (std::size_t lane = 0; lane < kDim; ++lane) {
      resp.data[lane] = static_cast<std::uint8_t>(entry.data[lane]);
    }
    resp.mask = static_cast<std::uint8_t>((1u << kDim) - 1u);
    resp.last = false;
    dmaread_val[bank] = 1;
    dmaread_bits[bank] = resp;
    if (dmaread_rdy[bank] != 0) {
      ++next_;
    }
  }

  void reset() {
    next_ = 0;
    done.reset(0);
    for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
      dmaread_val[bank].reset(0);
      dmaread_bits[bank].reset(DmaReadResp{});
    }
  }

 private:
  const std::vector<SpadPreloadRow>& rows_;
  std::size_t next_ = 0;
};

// Forwards ExCtrl's own completion signal into SmeshRS.completed (playing
// the single-source role ArbExLdStComplete plays for all three controllers
// in the full SmeshTop), and records the observed tag order for checking.
class CompletionForwarder : public Component {
  DECLARE_COMPONENT(CompletionForwarder);

 public:
  CompletionForwarder(std::string name, COMPONENT_CTOR) {
    UPDATE(update).reads(ex_completed_val, ex_completed_bits).writes(rs_completed);
  }

  Clock(clk);
  Input(bit, ex_completed_val);
  Input(SmeshRsTag, ex_completed_bits);
  FifoOutput(SmeshRsTag, rs_completed);

  const std::vector<SmeshRsTag>& observed() const { return observed_; }

  void update() {
    if (rs_completed.full() || ex_completed_val == 0) {
      return;
    }
    const auto tag = *ex_completed_bits;
    rs_completed.push(tag);
    observed_.push_back(tag);
  }

  void reset() { observed_.clear(); }

 private:
  std::vector<SmeshRsTag> observed_;
};

// Converts between ExCtrl's bank-local scratchpad/accumulator ports and
// the legacy full-address structs the shared arbiters still expect.
// Read-side mirrors SmeshTop::updateExCtrlReadReqAdapters() (private
// there); the write-side has no counterpart anywhere -- SmeshTop currently
// ties ExCtrl's write ports to a permanent-zero stub instead of wiring
// them to the arbiters at all, so this is the first place ExCtrl's
// writeback path is connected end-to-end to real Spad/Accum.
class ExCtrlMemAdapter : public Component {
  DECLARE_COMPONENT(ExCtrlMemAdapter);

 public:
  ExCtrlMemAdapter(std::string name, COMPONENT_CTOR) {
    UPDATE(updateReadAdapter)
        .reads(spad_read_req_bits)
        .writes(spad_read_req_legacy_bits);
    UPDATE(updateWriteAdapter)
        .reads(spad_write_val, spad_write_bits, accum_write_val, accum_write_bits)
        .writes(spad_exwrite_val, spad_exwrite_bits, accum_exwrite_val, accum_exwrite_bits);
  }

  Clock(clk);

  InputArray(SpadBankReadReq, spad_read_req_bits, kSpBanks);
  OutputArray(SpadReadReq, spad_read_req_legacy_bits, kSpBanks);

  InputArray(bit, spad_write_val, kSpBanks);
  InputArray(SpadBankWriteReq, spad_write_bits, kSpBanks);
  OutputArray(bit, spad_exwrite_val, kSpBanks);
  OutputArray(DmaReadResp, spad_exwrite_bits, kSpBanks);

  InputArray(bit, accum_write_val, kAccBanks);
  InputArray(AccumBankWriteReq, accum_write_bits, kAccBanks);
  OutputArray(bit, accum_exwrite_val, kAccBanks);
  OutputArray(DmaReadResp, accum_exwrite_bits, kAccBanks);

  void updateReadAdapter() {
    for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
      const auto req = *spad_read_req_bits[bank];
      SpadReadReq legacy{};
      legacy.laddr = makeSpAddr(static_cast<std::uint32_t>(bank * kSpBankRows) +
                                (static_cast<std::uint32_t>(req.addr) & kSpBankRowMask));
      legacy.from_dma = req.from_dma;
      spad_read_req_legacy_bits[bank] = legacy;
    }
  }

  void updateWriteAdapter() {
    for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
      spad_exwrite_val[bank] = spad_write_val[bank];
      if (spad_write_val[bank] == 0) {
        spad_exwrite_bits[bank] = DmaReadResp{};
        continue;
      }
      const auto req = *spad_write_bits[bank];
      DmaReadResp resp{};
      resp.laddr = makeSpAddr(static_cast<std::uint32_t>(bank * kSpBankRows) +
                              (req.addr & kSpBankRowMask));
      for (std::size_t lane = 0; lane < kDim; ++lane) {
        resp.data[lane] = static_cast<std::uint8_t>(req.data[lane]);
      }
      resp.mask = static_cast<std::uint8_t>(req.mask & 0xffu);
      resp.last = false;
      spad_exwrite_bits[bank] = resp;
    }

    for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
      accum_exwrite_val[bank] = accum_write_val[bank];
      if (accum_write_val[bank] == 0) {
        accum_exwrite_bits[bank] = DmaReadResp{};
        continue;
      }
      const auto req = *accum_write_bits[bank];
      DmaReadResp resp{};
      resp.laddr = makeAccAddr(static_cast<std::uint32_t>(bank * kAccBankRows) +
                               (req.addr & kAccBankRowMask),
                               /*do_accumulate=*/req.acc != 0);
      resp.has_acc_bitwidth = true;
      for (std::size_t lane = 0; lane < kDim; ++lane) {
        const auto value = static_cast<std::uint32_t>(req.data[lane]);
        for (std::size_t byte = 0; byte < sizeof(Acc); ++byte) {
          resp.data[lane * sizeof(Acc) + byte] =
              static_cast<std::uint8_t>((value >> (8 * byte)) & 0xffu);
        }
      }
      resp.mask = static_cast<std::uint8_t>(req.mask & 0xffu);
      resp.last = false;
      accum_exwrite_bits[bank] = resp;
    }
  }
};

// Constant-signal source for ports this test permanently ties off (the
// store-path read side, the zero-fill/full-width write sources, and
// ExCtrl's accumulator read side, none of which basic/mul_pre exercise).
class TieOff : public Component {
  DECLARE_COMPONENT(TieOff);

 public:
  TieOff(std::string name, COMPONENT_CTOR) {
    UPDATE(update).writes(zero_bit, one_bit, spad_read_req_zero, dma_read_resp_zero,
                          accum_read_resp_zero, accum_read_req_zero);
  }

  Clock(clk);
  Output(bit, zero_bit);
  Output(bit, one_bit);
  Output(SpadReadReq, spad_read_req_zero);
  Output(DmaReadResp, dma_read_resp_zero);
  Output(ExCtrlAccumReadResp, accum_read_resp_zero);
  Output(AccumReadReq, accum_read_req_zero);

  void update() {
    zero_bit = 0;
    one_bit = 1;
    spad_read_req_zero = SpadReadReq{};
    dma_read_resp_zero = DmaReadResp{};
    accum_read_resp_zero = ExCtrlAccumReadResp{};
    accum_read_req_zero = AccumReadReq{};
  }
};

} // namespace

StringParameter(test, "basic", "One rs_to_mem test name");
BoolParameter(list_tests, false, "List rs_to_mem tests and exit");

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  const auto tests = rsMemTestCases();
  if (list_tests) {
    for (const auto& t : tests) {
      std::printf("%-10s %s\n", t.name.c_str(), t.description.c_str());
    }
    return 0;
  }

  const auto found = std::find_if(tests.begin(), tests.end(),
                                  [](const RsMemTestCase& t) { return t.name == std::string(test); });
  if (found == tests.end()) {
    std::fprintf(stderr, "Unknown rs_to_mem test '%s'.\n", std::string(test).c_str());
    return 2;
  }
  const auto& tc = *found;

  // ----- Instantiate -----
  RsMemCmdDriver cmd_driver(tc.program, "CmdDriver");
  SpadPreloadDriver spad_preload(tc.spad_rows, "SpadPreload");
  SmeshCmdQueue cmd_queue("CmdQueue");
  SmeshUnrolledCmdQueue unrolled_queue("UnrolledCmdQueue");
  SmeshRS rs("RS");
  ExCtrl ex_ctrl("ExCtrl");
  CompletionForwarder completion_fwd("CompletionForwarder");
  ExCtrlMemAdapter mem_adapter("MemAdapter");
  TieOff tie("TieOff");
  Spad spad("Spad");
  Accum accum("Accum");

  std::array<ArbReadSpad*, kSpBanks> arb_read_spad{};
  std::array<ArbWriteSpad*, kSpBanks> arb_write_spad{};
  std::array<ArbRespSpad*, kSpBanks> arb_resp_spad{};
  std::array<SpadDmaReadPipe*, kSpBanks> spad_dma_pipe{};
  std::array<SpadExReadPipe*, kSpBanks> spad_ex_pipe{};
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    arb_read_spad[bank] = new ArbReadSpad("ArbReadSpad");
    arb_write_spad[bank] = new ArbWriteSpad("ArbWriteSpad");
    arb_resp_spad[bank] = new ArbRespSpad("ArbRespSpad");
    spad_dma_pipe[bank] = new SpadDmaReadPipe("SpadDmaReadPipe");
    spad_ex_pipe[bank] = new SpadExReadPipe("SpadExReadPipe");
  }
  std::array<ArbWriteAccum*, kAccBanks> arb_write_accum{};
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    arb_write_accum[bank] = new ArbWriteAccum("ArbWriteAccum");
  }

  // ----- Wire: host command front door -----
  cmd_queue.cmd_valid << cmd_driver.cmd_valid;
  cmd_queue.cmd_bits << cmd_driver.cmd_bits;
  cmd_driver.cmd_ready << cmd_queue.cmd_ready;
  cmd_driver.preload_done << spad_preload.done;

  unrolled_queue.cmd_in << cmd_queue.cmd_out;
  rs.alloc_in << unrolled_queue.cmd_out;
  rs.issue_ld.sendToBitBucket();
  rs.issue_st.sendToBitBucket();
  rs.setExecuteIssuePortEnabled(true);

  // ----- Wire: RS <-> ExCtrl <-> completion loop -----
  ex_ctrl.cmd_in << rs.issue_ex;
  completion_fwd.ex_completed_val << ex_ctrl.completed_val;
  completion_fwd.ex_completed_bits << ex_ctrl.completed_bits;
  rs.completed << completion_fwd.rs_completed;

  // ----- Wire: ExCtrl accumulator reads, permanently tied off (never
  // exercised by basic/mul_pre -- see class-level comment on TieOff). -----
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    ex_ctrl.accum_read_req_rdy[bank] << tie.one_bit;
    ex_ctrl.accum_read_resp_val[bank] << tie.zero_bit;
    ex_ctrl.accum_read_resp_bits[bank] << tie.accum_read_resp_zero;
  }

  // ----- Wire: ExCtrl spad read path -----
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    mem_adapter.spad_read_req_bits[bank] << ex_ctrl.spad_read_req_bits[bank];

    arb_read_spad[bank]->exread_val << ex_ctrl.spad_read_req_val[bank];
    arb_read_spad[bank]->exread_bits << mem_adapter.spad_read_req_legacy_bits[bank];
    ex_ctrl.spad_read_req_rdy[bank] << arb_read_spad[bank]->exread_rdy;
    arb_read_spad[bank]->dmawrite_val << tie.zero_bit;
    arb_read_spad[bank]->dmawrite_bits << tie.spad_read_req_zero;
    arb_read_spad[bank]->read_req_rdy << spad.read_req_rdy_bnk[bank];
    spad.read_req_val_bnk[bank] << arb_read_spad[bank]->read_req_val;
    spad.read_req_bits_bnk[bank] << arb_read_spad[bank]->read_req_bits;

    spad_dma_pipe[bank]->resp_val << spad.read_resp_val_bnk[bank];
    spad_dma_pipe[bank]->resp_bits << spad.read_resp_bits_bnk[bank];
    spad_dma_pipe[bank]->out_rdy << tie.one_bit;
    spad_ex_pipe[bank]->resp_val << spad.read_resp_val_bnk[bank];
    spad_ex_pipe[bank]->resp_bits << spad.read_resp_bits_bnk[bank];
    ex_ctrl.spad_read_resp_val[bank] << spad_ex_pipe[bank]->out_val;
    ex_ctrl.spad_read_resp_bits[bank] << spad_ex_pipe[bank]->out_bits;
    spad_ex_pipe[bank]->out_rdy << ex_ctrl.spad_read_resp_rdy[bank];
    arb_resp_spad[bank]->read_resp_val << spad.read_resp_val_bnk[bank];
    arb_resp_spad[bank]->read_resp_bits << spad.read_resp_bits_bnk[bank];
    arb_resp_spad[bank]->dma_resp_rdy << spad_dma_pipe[bank]->resp_rdy;
    arb_resp_spad[bank]->ex_resp_rdy << spad_ex_pipe[bank]->resp_rdy;
    spad.read_resp_rdy_bnk[bank] << arb_resp_spad[bank]->read_resp_rdy;
  }

  // ----- Wire: spad write path (preload channel + real ExCtrl writeback) -----
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    mem_adapter.spad_write_val[bank] << ex_ctrl.spad_write_val[bank];
    mem_adapter.spad_write_bits[bank] << ex_ctrl.spad_write_bits[bank];

    arb_write_spad[bank]->exwrite_val << mem_adapter.spad_exwrite_val[bank];
    arb_write_spad[bank]->exwrite_bits << mem_adapter.spad_exwrite_bits[bank];
    ex_ctrl.spad_write_rdy[bank] << arb_write_spad[bank]->exwrite_rdy;
    arb_write_spad[bank]->dmaread_val << spad_preload.dmaread_val[bank];
    arb_write_spad[bank]->dmaread_bits << spad_preload.dmaread_bits[bank];
    spad_preload.dmaread_rdy[bank] << arb_write_spad[bank]->dmaread_rdy;
    arb_write_spad[bank]->zerowrite_val << tie.zero_bit;
    arb_write_spad[bank]->zerowrite_bits << tie.dma_read_resp_zero;
    arb_write_spad[bank]->write_rdy << spad.write_rdy_bnk[bank];
    spad.write_val_bnk[bank] << arb_write_spad[bank]->write_val;
    spad.write_bits_bnk[bank] << arb_write_spad[bank]->write_bits;
  }
  spad.dma_resp.sendToBitBucket();

  // ----- Wire: accum write path (real ExCtrl writeback only) -----
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    mem_adapter.accum_write_val[bank] << ex_ctrl.accum_write_val[bank];
    mem_adapter.accum_write_bits[bank] << ex_ctrl.accum_write_bits[bank];

    arb_write_accum[bank]->exwrite_val << mem_adapter.accum_exwrite_val[bank];
    arb_write_accum[bank]->exwrite_bits << mem_adapter.accum_exwrite_bits[bank];
    ex_ctrl.accum_write_rdy[bank] << arb_write_accum[bank]->exwrite_rdy;
    arb_write_accum[bank]->dmaread_val << tie.zero_bit;
    arb_write_accum[bank]->dmaread_bits << tie.dma_read_resp_zero;
    arb_write_accum[bank]->dmaread_full_val << tie.zero_bit;
    arb_write_accum[bank]->dmaread_full_bits << tie.dma_read_resp_zero;
    arb_write_accum[bank]->zerowrite_val << tie.zero_bit;
    arb_write_accum[bank]->zerowrite_bits << tie.dma_read_resp_zero;
    arb_write_accum[bank]->write_rdy << accum.write_rdy_bnk[bank];
    accum.write_val_bnk[bank] << arb_write_accum[bank]->write_val;
    accum.write_bits_bnk[bank] << arb_write_accum[bank]->write_bits;

    accum.read_req_val_bnk[bank] << tie.zero_bit;
    accum.read_req_bits_bnk[bank] << tie.accum_read_req_zero;
    accum.read_resp_rdy_bnk[bank] << tie.one_bit;
  }
  accum.dma_resp.sendToBitBucket();

  // ----- Clock -----
  Clock clk;
  cmd_driver.clk << clk;
  spad_preload.clk << clk;
  cmd_queue.clk << clk;
  unrolled_queue.clk << clk;
  rs.clk << clk;
  ex_ctrl.clk << clk;
  completion_fwd.clk << clk;
  mem_adapter.clk << clk;
  tie.clk << clk;
  spad.clk << clk;
  accum.clk << clk;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    arb_read_spad[bank]->clk << clk;
    arb_write_spad[bank]->clk << clk;
    arb_resp_spad[bank]->clk << clk;
    spad_dma_pipe[bank]->clk << clk;
    spad_ex_pipe[bank]->clk << clk;
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    arb_write_accum[bank]->clk << clk;
  }
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();

  int drain_count = 0;
  for (int cycle = 0; cycle < tc.max_cycles; ++cycle) {
    Sim::run();
    const bool quiet = cmd_driver.done() && rs.empty();
    drain_count = quiet ? drain_count + 1 : 0;
    if (drain_count >= tc.drain_cycles) {
      break;
    }
  }

  // ----- Check -----
  bool completion_ok = completion_fwd.observed() == tc.expected_completion_order;

  bool result_ok = true;
  for (const auto& expected : tc.expected_results) {
    for (std::size_t r = 0; r < expected.rows.size(); ++r) {
      const auto addr = expected.base + static_cast<std::uint32_t>(r);
      const auto& actual = accum.row(addr);
      for (std::size_t c = 0; c < kDim; ++c) {
        if (actual[c] != expected.rows[r][c]) {
          result_ok = false;
        }
      }
    }
  }

  const bool passed = completion_ok && result_ok;
  std::printf("[RS_TO_MEM_SUITE] %s %s\n", passed ? "PASS" : "FAIL", tc.name.c_str());
  if (!passed) {
    std::printf("  completion_ok=%u result_ok=%u rs_empty=%u cmd_driver_done=%u\n",
                completion_ok ? 1u : 0u, result_ok ? 1u : 0u,
                rs.empty() ? 1u : 0u, cmd_driver.done() ? 1u : 0u);
    std::printf("  expected completions:");
    for (const auto tagv : tc.expected_completion_order) {
      std::printf(" %u", static_cast<unsigned>(tagv));
    }
    std::printf("\n  observed completions:");
    for (const auto tagv : completion_fwd.observed()) {
      std::printf(" %u", static_cast<unsigned>(tagv));
    }
    std::printf("\n");
    for (const auto& expected : tc.expected_results) {
      for (std::size_t r = 0; r < expected.rows.size(); ++r) {
        const auto addr = expected.base + static_cast<std::uint32_t>(r);
        const auto& actual = accum.row(addr);
        std::printf("  accum[%u] expected={%d,%d,%d,%d} actual={%d,%d,%d,%d}\n",
                    addr.data(),
                    static_cast<int>(expected.rows[r][0]), static_cast<int>(expected.rows[r][1]),
                    static_cast<int>(expected.rows[r][2]), static_cast<int>(expected.rows[r][3]),
                    static_cast<int>(actual[0]), static_cast<int>(actual[1]),
                    static_cast<int>(actual[2]), static_cast<int>(actual[3]));
      }
    }
  }

  for (auto* arb : arb_read_spad) { delete arb; }
  for (auto* arb : arb_write_spad) { delete arb; }
  for (auto* arb : arb_resp_spad) { delete arb; }
  for (auto* pipe : spad_dma_pipe) { delete pipe; }
  for (auto* pipe : spad_ex_pipe) { delete pipe; }
  for (auto* arb : arb_write_accum) { delete arb; }

  return passed ? 0 : 1;
}
