// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_test_cases.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 19 2026
/*
rs_to_mem test scenarios. Each program is issued as raw SmeshCmds through
the real SmeshCmdQueue -> LoopMatmul -> SmeshUnrolledCmdQueue -> SmeshRS -> ExCtrl path
(see tb_rs_to_mem_harness.hpp), so completion tags are assigned by the
real SmeshRS at allocation time rather than hand-picked -- since our
programs are issued one command per cycle and RS allocates strictly in
arrival order, tags come out sequentially as 0, 1, 2, ... in program
order. Completion order is not fixed: mesh and pending command completions
can be reported on different cycles depending on memory timing. Each case
checks that every expected tag appears exactly once.
*/

#include "tb_rs_to_mem_test_cases.hpp"

namespace smesh {
namespace tb {

namespace {

std::vector<SpadPreloadRow> spadRows(std::uint32_t base,
                                     std::initializer_list<MeshInputRow> rows) {
  std::vector<SpadPreloadRow> out;
  std::uint32_t addr = base;
  for (const auto& data : rows) {
    out.push_back(SpadPreloadRow{makeSpAddr(addr), data});
    ++addr;
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

// C = A*B + D, one output row at a time -- the same arithmetic ExCtrl's
// mesh is expected to compute, recomputed independently here so the test
// isn't just checking ExCtrl against itself.
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

std::vector<std::uint8_t> rowBytes(const MeshInputRow& values) {
  std::vector<std::uint8_t> bytes;
  for (const auto value : values) bytes.push_back(static_cast<std::uint8_t>(value));
  return bytes;
}

SmeshCmd loopCommand(SmeshFunct funct, std::uint64_t rs1, std::uint64_t rs2) {
  SmeshCmd cmd{};
  cmd.funct = static_cast<std::uint32_t>(funct);
  cmd.rs1 = rs1;
  cmd.rs2 = rs2;
  return cmd;
}

std::vector<SmeshCmd> dmaLoopProgram(std::uint16_t i_tiles, std::uint16_t k_tiles,
                                     std::uint64_t a_base, std::uint64_t b_base,
                                     std::uint64_t d_base, std::uint64_t c_base) {
  const auto config_load = [](unsigned state, std::uint64_t stride, bool shrink) {
    const auto rs1 = packConfig(ConfigKind::Load, state, kDim) |
                     (shrink ? (std::uint64_t{1} << 2) : 0);
    return loopCommand(SmeshFunct::Config, rs1, stride);
  };
  const auto a_stride = static_cast<std::uint64_t>(k_tiles) * kDim;
  const auto row_stride = static_cast<std::uint64_t>(kDim);
  return {
      config_load(0, a_stride, false), config_load(1, row_stride, false),
      config_load(2, row_stride, true), configExCmd(),
      loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store),
                  (std::uint64_t{1} << 32) | row_stride),
      loopCommand(SmeshFunct::LoopWsBounds, 0,
                  (std::uint64_t{k_tiles} << 32) | (std::uint64_t{1} << 16) | i_tiles),
      loopCommand(SmeshFunct::LoopWsAddrsAb, a_base, b_base),
      loopCommand(SmeshFunct::LoopWsAddrsDc, d_base, c_base),
      loopCommand(SmeshFunct::LoopWsStridesAb, a_stride, row_stride),
      loopCommand(SmeshFunct::LoopWsStridesDc, row_stride, row_stride),
      loopCommand(SmeshFunct::LoopWs, (std::uint64_t{2} << 16) | (1ull << 2) | 1ull, 0),
  };
}

} // namespace

// ********************* TEST CASES *********************

// "basic": exercises the plain sequential path -- a standalone PRELOAD
// followed by a standalone COMPUTE_FLIP, then a trailing standalone
// COMPUTE_STAY that reuses the same weights but has no PRELOAD after it
// (so it's expected to complete without ever writing a result, since a
// standalone COMPUTE has no destination of its own -- see doc/notes.md).
// This is the real-RS equivalent of ex_ctrl's makeBasicFlipStayTest();
// same matrices, same memory layout, issued as SmeshCmds through the real
// command-queue+RS front door instead of hand-tagged SmeshIssues.
//
// Program order: CONFIG, PRELOAD, COMPUTE_FLIP, COMPUTE_STAY -- allocated
// by RS as tags 0,1,2,3 in that order. Check all four completions, but do
// not require an order: mesh and pending command completions can be
// reported on different cycles.
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
  test.expected_completion_tags = {0, 1, 2, 3};

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

// "mul_pre": exercises the overlapping COMPUTE+PRELOAD path -- the second
// PRELOAD is issued right after the first COMPUTE_FLIP, so ExCtrl accepts
// them together as a single "mul_pre" operation (weights for the second
// matmul load into the array's other stationary-weight register while the
// first matmul is still computing). Produces two independent results.
// Real-RS equivalent of ex_ctrl's makeComputePreloadOverlapTest().
//
// Program order: CONFIG, PRELOAD0, COMPUTE_FLIP0, PRELOAD1, COMPUTE_FLIP1
// -- tags 0..4. Check all five completions without assuming their relative
// order; mesh and pending completions can interleave as timing changes.
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
  test.expected_completion_tags = {0, 1, 2, 3, 4};

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

// "concurrent_banks": identical program to "mul_pre" above -- its A/D-as-B/
// preload-weight-as-D addresses already land in three distinct Spad banks,
// which produces real 3-bank-concurrent reads, but "mul_pre" never asserts
// on that (it only checks completion tags and final Accum values, and was
// laid out that way to test command *pairing*, not concurrency). Reusing
// the same program and adding min_concurrent_spad_banks turns an
// incidentally-observed behavior into a machine-checked one: if a future
// change to address layout, arbitration, or Spad itself silently regressed
// real cross-bank concurrency back to one-bank-at-a-time, this is the test
// that would actually catch it.
RsMemTestCase makeConcurrentBanksCase() {
  RsMemTestCase test = makeMulPreCase();
  test.name = "concurrent_banks";
  test.description = "Same program as mul_pre, but asserts the 3-bank-concurrent Spad reads it produces";
  test.min_concurrent_spad_banks = 3;
  return test;
}

// "same_bank_serializes": the negative counterpart to concurrent_banks.
// The compute's input and addend deliberately alias the exact same Spad
// address (so they always share a bank), while the overlapping preload's
// weight sits in a separate bank. ExCtrlReadPriority's same-bank wait rule
// should force the aliased pair to serialize across two cycles instead of
// firing together, so no cycle should ever see more than 2 banks active at
// once (the preload's weight bank plus whichever of the pair currently has
// priority) -- confirms same-bank conflicts still correctly serialize now
// that cross-bank reads are genuinely concurrent, rather than assuming the
// old single-bank-at-a-time Spad was quietly doing that job for free.
RsMemTestCase makeSameBankSerializesCase() {
  RsMemTestCase test{};
  test.name = "same_bank_serializes";
  test.description = "Compute's A and addend alias one Spad bank -- checks read-priority conflicts still serialize";

  const auto a0 = makeSpAddr(0);  // bank 0 -- shared by the compute's A and its addend
  const auto b1 = makeSpAddr(4);  // bank 1 -- overlapping preload's weight
  const auto b0 = makeSpAddr(8);  // bank 2 -- first preload's weight
  const auto c0 = makeAccAddr(0);
  const auto c1 = makeAccAddr(8);

  const std::vector<MeshInputRow> a0_rows = {
      row({0, 1, 2, 3}), row({4, 5, 6, 7}), row({8, 9, 10, 11}), row({12, 13, 14, 15})};
  const std::vector<MeshInputRow> b1_rows = {
      row({16, 17, 18, 19}), row({20, 21, 22, 23}), row({24, 25, 26, 27}), row({28, 29, 30, 31})};
  const std::vector<MeshInputRow> b0_rows = {
      row({32, 33, 34, 35}), row({36, 37, 38, 39}), row({40, 41, 42, 43}), row({44, 45, 46, 47})};

  test.spad_rows = spadRows(0, {a0_rows[0], a0_rows[1], a0_rows[2], a0_rows[3]});
  const auto b1v = spadRows(4, {b1_rows[0], b1_rows[1], b1_rows[2], b1_rows[3]});
  const auto b0v = spadRows(8, {b0_rows[0], b0_rows[1], b0_rows[2], b0_rows[3]});
  test.spad_rows.insert(test.spad_rows.end(), b1v.begin(), b1v.end());
  test.spad_rows.insert(test.spad_rows.end(), b0v.begin(), b0v.end());

  test.program = {
      configExCmd(),
      preloadCmd(b0, c0),
      computeFlipCmd(a0, a0),  // addend aliases the input address -- forces a same-bank conflict
      preloadCmd(b1, c1),
      computeFlipCmd(a0, a0),
  };
  test.expected_completion_tags = {0, 1, 2, 3, 4};
  test.min_concurrent_spad_banks = 2;  // preload's weight bank should still overlap with the winning half of the pair
  test.max_concurrent_spad_banks = 2;  // the aliased pair must never both fire the same cycle

  const auto aarr = toArray(a0_rows);
  const auto b0arr = toArray(b0_rows);
  const auto b1arr = toArray(b1_rows);
  std::vector<MeshAccumRow> c0_rows;
  std::vector<MeshAccumRow> c1_rows;
  for (std::size_t r = 0; r < kDim; ++r) {
    c0_rows.push_back(matmulRow(aarr, b0arr, aarr, r));  // D operand is A's own data
    c1_rows.push_back(matmulRow(aarr, b1arr, aarr, r));
  }
  test.expected_results = {ExpectedAccumResult{c0, c0_rows},
                           ExpectedAccumResult{c1, c1_rows}};
  return test;
}

// starts with data already in SPAD
RsMemTestCase makeLoopWsCase() {
  RsMemTestCase test{};
  test.name = "loop_ws";
  test.description = "one LOOP_WS through Smesh, RS, ExCtrl, and Accum";
  test.max_cycles = 300;
  test.expect_loop_release = true;

  const std::array<MeshInputRow, kDim> a = {
      row({1, 2, 3, 4}), row({5, 6, 7, 8}),
      row({9, 10, 11, 12}), row({13, 14, 15, 16})};
  const std::array<MeshInputRow, kDim> b = {
      row({1, 0, 0, 0}), row({0, 1, 0, 0}),
      row({0, 0, 1, 0}), row({0, 0, 0, 1})};
  const auto b_start = static_cast<std::uint32_t>(kSpRows / 2 - kDim);
  for (std::size_t r = 0; r < kDim; ++r) {
    test.spad_rows.push_back(SpadPreloadRow{makeSpAddr(static_cast<std::uint32_t>(r)), a[r]});
    test.spad_rows.push_back(SpadPreloadRow{makeSpAddr(b_start + static_cast<std::uint32_t>(r)), b[r]});
  }

  const auto loopCmd = [](SmeshFunct funct, std::uint64_t rs1, std::uint64_t rs2) {
    SmeshCmd cmd{};
    cmd.funct = static_cast<std::uint32_t>(funct);
    cmd.rs1 = rs1;
    cmd.rs2 = rs2;
    return cmd;
  };
  test.program = {
      configExCmd(),
      loopCmd(SmeshFunct::LoopWsBounds, 0, (1ull << 32) | (1ull << 16) | 1ull),
      loopCmd(SmeshFunct::LoopWsAddrsAb, 0, 0),
      loopCmd(SmeshFunct::LoopWsAddrsDc, 0, 0),
      loopCmd(SmeshFunct::LoopWsStridesAb, kDim, kDim),
      loopCmd(SmeshFunct::LoopWsStridesDc, kDim, kDim),
      loopCmd(SmeshFunct::LoopWs, 0, 0),
  };
  test.expected_completion_tags = {0, 1, 2};
  std::vector<MeshAccumRow> c;
  for (std::size_t r = 0; r < kDim; ++r) {
    c.push_back(matmulRow(a, b, {}, r));
  }
  test.expected_results = {ExpectedAccumResult{makeAccAddr(0), c}};
  return test;
}

// "loop_ws_dma": exercises the LOOP_WS path with DRAM loads and stores 
// Seeds 4x4 A, identity B, and all-ones D in DRAM, runs a LOOP_WS through Smesh,
// and checks the loaded rows, accumulator result, completions, and C written
// back to DRAM.
RsMemTestCase makeLoopWsDmaCase() {
  RsMemTestCase test{};
  test.name = "loop_ws_dma";
  test.description = "LOOP_WS loads A/B/D from DRAM and stores C to DRAM";
  test.max_cycles = 600;
  test.drain_cycles = 16;
  test.expect_loop_release = true;

  constexpr std::uint64_t a_base = 0x80004000;
  constexpr std::uint64_t b_base = 0x80005000;
  constexpr std::uint64_t d_base = 0x80006000;
  constexpr std::uint64_t c_base = 0x80007000;
  const std::array<MeshInputRow, kDim> a = {
      row({1, 2, 3, 4}), row({5, 6, 7, 8}),
      row({9, 10, 11, 12}), row({13, 14, 15, 16})};
  const std::array<MeshInputRow, kDim> b = {
      row({1, 0, 0, 0}), row({0, 1, 0, 0}),
      row({0, 0, 1, 0}), row({0, 0, 0, 1})};
  const std::array<MeshInputRow, kDim> d = {
      row({1, 1, 1, 1}), row({1, 1, 1, 1}),
      row({1, 1, 1, 1}), row({1, 1, 1, 1})};

  const auto bytes = [](const MeshInputRow& values) {
    std::vector<std::uint8_t> out;
    for (const auto value : values) out.push_back(static_cast<std::uint8_t>(value));
    return out;
  };
  const auto b_start = static_cast<std::uint32_t>(kSpRows / 2 - kDim);
  std::vector<MeshAccumRow> c;
  for (std::size_t r = 0; r < kDim; ++r) {
    const auto offset = r * kDim;
    test.dram_initial.push_back({a_base + offset, bytes(a[r])});
    test.dram_initial.push_back({b_base + offset, bytes(b[r])});
    test.dram_initial.push_back({d_base + offset, bytes(d[r])});
    test.expected_loaded_spad_rows.push_back({makeSpAddr(static_cast<std::uint32_t>(r)), a[r]});
    test.expected_loaded_spad_rows.push_back({makeSpAddr(b_start + static_cast<std::uint32_t>(r)), b[r]});
    c.push_back(matmulRow(a, b, d, r));
    std::vector<std::uint8_t> result;
    for (const auto value : c.back()) result.push_back(static_cast<std::uint8_t>(value));
    test.expected_dram.push_back({c_base + offset, result});
  }

  const auto command = [](SmeshFunct funct, std::uint64_t rs1, std::uint64_t rs2) {
    SmeshCmd cmd{};
    cmd.funct = static_cast<std::uint32_t>(funct);
    cmd.rs1 = rs1;
    cmd.rs2 = rs2;
    return cmd;
  };
  const auto configLoad = [&](unsigned state, bool shrink) {
    const auto rs1 = packConfig(ConfigKind::Load, state, kDim) |
                     (shrink ? (std::uint64_t{1} << 2) : 0);
    return command(SmeshFunct::Config, rs1, kDim);
  };
  test.program = {
      configLoad(0, false), configLoad(1, false), configLoad(2, true),
      configExCmd(),
      command(SmeshFunct::Config, packConfig(ConfigKind::Store),
              (std::uint64_t{1} << 32) | kDim),
      command(SmeshFunct::LoopWsBounds, 0, (1ull << 32) | (1ull << 16) | 1ull),
      command(SmeshFunct::LoopWsAddrsAb, a_base, b_base),
      command(SmeshFunct::LoopWsAddrsDc, d_base, c_base),
      command(SmeshFunct::LoopWsStridesAb, kDim, kDim),
      command(SmeshFunct::LoopWsStridesDc, kDim, kDim),
      command(SmeshFunct::LoopWs, (1ull << 2) | 1ull, 0),
  };
  test.expected_completion_tags = {3, 5, 6, 7, 8, 9, 10};
  test.expected_results = {ExpectedAccumResult{makeAccAddr(0), c}};
  return test;
}

// Two output tiles share B but use distinct A/D tiles and two destinct C destinations.
RsMemTestCase makeLoopWsDmaI2Case() {
  RsMemTestCase test{};
  test.name = "loop_ws_dma_i2";
  test.description = "LOOP_WS I=2 J=1 K=1 writes two C tiles to DRAM";
  test.max_cycles = 900;
  test.drain_cycles = 16;
  test.expect_loop_release = true;

  constexpr std::uint64_t a_base = 0x80004000;
  constexpr std::uint64_t b_base = 0x80005000;
  constexpr std::uint64_t d_base = 0x80006000;
  constexpr std::uint64_t c_base = 0x80007000;
  test.program = dmaLoopProgram(2, 1, a_base, b_base, d_base, c_base);

  const auto b_start = static_cast<std::uint32_t>(kSpRows - kDim);
  std::vector<MeshAccumRow> c_rows;
  for (std::size_t r = 0; r < 2 * kDim; ++r) {
    MeshInputRow a{};
    MeshInputRow d{};
    MeshAccumRow c{};
    for (std::size_t col = 0; col < kDim; ++col) {
      a[col] = static_cast<Elem>(1 + r * kDim + col);
      d[col] = 1;
      c[col] = static_cast<Acc>(a[col]) + 1;
    }
    const auto offset = r * kDim;
    test.dram_initial.push_back({a_base + offset, rowBytes(a)});
    test.dram_initial.push_back({d_base + offset, rowBytes(d)});
    test.expected_loaded_spad_rows.push_back({makeSpAddr(static_cast<std::uint32_t>(r)), a});
    c_rows.push_back(c);
    MeshInputRow narrow_c{};
    for (std::size_t col = 0; col < kDim; ++col) narrow_c[col] = static_cast<Elem>(c[col]);
    test.expected_dram.push_back({c_base + offset, rowBytes(narrow_c)});
  }
  for (std::size_t r = 0; r < kDim; ++r) {
    MeshInputRow b{};
    b[r] = 1;
    test.dram_initial.push_back({b_base + r * kDim, rowBytes(b)});
    test.expected_loaded_spad_rows.push_back({makeSpAddr(b_start + static_cast<std::uint32_t>(r)), b});
  }
  test.expected_results = {ExpectedAccumResult{makeAccAddr(0), c_rows}};
  test.expected_completion_tags.push_back(3);
  for (std::uint8_t tag = 5; tag <= 15; ++tag) test.expected_completion_tags.push_back(tag);
  return test;
}

// Two K tiles contribute to one C tile; the second product must accumulate.
// That is, expected results is: D + A0*B0 + A1*B1
RsMemTestCase makeLoopWsDmaK2Case() {
  RsMemTestCase test{};
  test.name = "loop_ws_dma_k2";
  test.description = "LOOP_WS I=1 J=1 K=2 accumulates two products into C";
  test.max_cycles = 900;
  test.drain_cycles = 16;
  test.expect_loop_release = true;

  constexpr std::uint64_t a_base = 0x80004000;
  constexpr std::uint64_t b_base = 0x80005000;
  constexpr std::uint64_t d_base = 0x80006000;
  constexpr std::uint64_t c_base = 0x80007000;
  test.program = dmaLoopProgram(1, 2, a_base, b_base, d_base, c_base);

  const auto b_start = static_cast<std::uint32_t>(kSpRows - 2 * kDim);
  std::vector<MeshAccumRow> c_rows;
  for (std::size_t r = 0; r < kDim; ++r) {
    std::vector<std::uint8_t> a_bytes;
    MeshAccumRow c{};
    MeshInputRow d{};
    for (std::size_t col = 0; col < 2 * kDim; ++col) {
      a_bytes.push_back(static_cast<std::uint8_t>(1 + r * 2 * kDim + col));
    }
    for (std::size_t col = 0; col < kDim; ++col) {
      d[col] = 1;
      c[col] = 1 + a_bytes[col] + 2 * a_bytes[kDim + col];
    }
    test.dram_initial.push_back({a_base + r * 2 * kDim, a_bytes});
    test.dram_initial.push_back({d_base + r * kDim, rowBytes(d)});
    for (std::size_t k = 0; k < 2; ++k) {
      MeshInputRow a_tile_row{};
      for (std::size_t col = 0; col < kDim; ++col) {
        a_tile_row[col] = static_cast<Elem>(a_bytes[k * kDim + col]);
      }
      test.expected_loaded_spad_rows.push_back(
          {makeSpAddr(static_cast<std::uint32_t>(k * kDim + r)), a_tile_row});
    }
    c_rows.push_back(c);
    MeshInputRow narrow_c{};
    for (std::size_t col = 0; col < kDim; ++col) narrow_c[col] = static_cast<Elem>(c[col]);
    test.expected_dram.push_back({c_base + r * kDim, rowBytes(narrow_c)});
  }
  for (std::size_t k = 0; k < 2; ++k) {
    for (std::size_t r = 0; r < kDim; ++r) {
      MeshInputRow b{};
      b[r] = static_cast<Elem>(k == 0 ? 1 : 2);
      test.dram_initial.push_back({b_base + (k * kDim + r) * kDim, rowBytes(b)});
      test.expected_loaded_spad_rows.push_back(
          {makeSpAddr(b_start + static_cast<std::uint32_t>(k * kDim + r)), b});
    }
  }
  test.expected_results = {ExpectedAccumResult{makeAccAddr(0), c_rows}};
  test.expected_completion_tags.push_back(3);
  for (std::uint8_t tag = 5; tag <= 13; ++tag) test.expected_completion_tags.push_back(tag);
  return test;
}

// One 4-element accumulator row occupies 16 DRAM bytes, or two memory beats.
RsMemTestCase makeFullWidthAccumLoadCase() {
  RsMemTestCase test{};
  test.name = "full_width_accum_load";
  test.description = "LOAD full-width accumulator row across two DRAM beats";
  test.max_cycles = 160;

  constexpr std::uint64_t dram_base = 0x80008000;
  const auto destination = makeAccAddr(8);
  const MeshAccumRow values{0x12345678, 0x00000123, 0x76543210, 0x00010002};
  std::vector<std::uint8_t> bytes;
  for (const auto value : values) {
    const auto word = static_cast<std::uint32_t>(value);
    for (unsigned byte = 0; byte < sizeof(Acc); ++byte) {
      bytes.push_back(static_cast<std::uint8_t>(word >> (8 * byte)));
    }
  }
  test.dram_initial.push_back({dram_base, bytes});

  test.program = {
      loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Load, 0, kDim), bytes.size()),
      loopCommand(SmeshFunct::Mvin, dram_base,
                  packLocal(destination, MatrixShape{1, kDim})),
  };
  test.expected_completion_tags = {1};
  test.expected_results = {ExpectedAccumResult{destination, {values}}};
  return test;
}

std::vector<RsMemTestCase> rsMemTestCases() {
  return {makeBasicCase(), makeMulPreCase(), makeConcurrentBanksCase(),
          makeSameBankSerializesCase(), makeLoopWsCase(), makeLoopWsDmaCase(),
          makeLoopWsDmaI2Case(), makeLoopWsDmaK2Case(), makeFullWidthAccumLoadCase()};
}

} // namespace tb
} // namespace smesh
