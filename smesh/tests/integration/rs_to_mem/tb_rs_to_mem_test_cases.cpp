// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_test_cases.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 19 2026
/*
rs_to_mem test scenarios. Each program is issued as raw SmeshCmds through
the real SmeshCmdQueue -> LoopConv -> LoopMatmul -> SmeshUnrolledCmdQueue -> SmeshRS path
(see tb_rs_to_mem_harness.hpp), so completion tags are assigned by the
real SmeshRS at allocation time rather than hand-picked -- since our
programs are issued one command per cycle and RS allocates strictly in
arrival order, tags come out sequentially as 0, 1, 2, ... in program
order. Completion order is not fixed: mesh and pending command completions
can be reported on different cycles depending on memory timing. Each case
checks that every expected tag appears exactly once.
*/

#include "tb_rs_to_mem_test_cases.hpp"

#include <utility>

namespace smesh {
namespace tb {

namespace {

std::vector<SpadPreloadRow> spadRows(std::uint32_t base, std::initializer_list<MeshInputRow> rows) {
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
  cmd.rs1   = u64(packConfigExRs1(/*a_stride=*/1));
  cmd.rs2   = u64(packConfigExRs2(/*c_stride=*/1));
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
    const auto rs1 = packConfigLoadRs1(state, kDim, kMvinScaleIdentityBits, shrink);
    return loopCommand(SmeshFunct::Config, rs1, stride);
  };
  const auto a_stride = static_cast<std::uint64_t>(k_tiles) * kDim;
  const auto row_stride = static_cast<std::uint64_t>(kDim);
  return {
      config_load(0, a_stride, false), config_load(1, row_stride, false),
      config_load(2, row_stride, true), configExCmd(),
      loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store),
                  (std::uint64_t{kAccScaleIdentityBits} << 32) | row_stride),
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
  const auto b0 = makeSpAddr(kSpBankRows);      // bank 1 (doubles as math D for COMPUTE_FLIP)
  const auto a1 = makeSpAddr(2 * kSpBankRows);  // bank 2
  const auto a0 = makeSpAddr(3 * kSpBankRows);  // bank 3
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
  const auto b0v = spadRows(b0.full_sp_addr(), {b0_rows[0], b0_rows[1], b0_rows[2], b0_rows[3]});
  const auto a1v = spadRows(a1.full_sp_addr(), {a1_rows[0], a1_rows[1], a1_rows[2], a1_rows[3]});
  const auto a0v = spadRows(a0.full_sp_addr(), {a0_rows[0], a0_rows[1], a0_rows[2], a0_rows[3]});
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
  const auto d0 = makeSpAddr(kSpBankRows);      // bank 1 (D0 == D1)
  const auto b1 = makeSpAddr(2 * kSpBankRows);  // bank 2
  const auto b0 = makeSpAddr(3 * kSpBankRows);  // bank 3
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
  const auto d0v = spadRows(d0.full_sp_addr(), {d0_rows[0], d0_rows[1], d0_rows[2], d0_rows[3]});
  const auto b1v = spadRows(b1.full_sp_addr(), {b1_rows[0], b1_rows[1], b1_rows[2], b1_rows[3]});
  const auto b0v = spadRows(b0.full_sp_addr(), {b0_rows[0], b0_rows[1], b0_rows[2], b0_rows[3]});
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
  const auto b1 = makeSpAddr(kSpBankRows);      // bank 1 -- overlapping preload's weight
  const auto b0 = makeSpAddr(2 * kSpBankRows);  // bank 2 -- first preload's weight
  const auto c0 = makeAccAddr(0);
  const auto c1 = makeAccAddr(8);

  const std::vector<MeshInputRow> a0_rows = {
      row({0, 1, 2, 3}), row({4, 5, 6, 7}), row({8, 9, 10, 11}), row({12, 13, 14, 15})};
  const std::vector<MeshInputRow> b1_rows = {
      row({16, 17, 18, 19}), row({20, 21, 22, 23}), row({24, 25, 26, 27}), row({28, 29, 30, 31})};
  const std::vector<MeshInputRow> b0_rows = {
      row({32, 33, 34, 35}), row({36, 37, 38, 39}), row({40, 41, 42, 43}), row({44, 45, 46, 47})};

  test.spad_rows = spadRows(0, {a0_rows[0], a0_rows[1], a0_rows[2], a0_rows[3]});
  const auto b1v = spadRows(b1.full_sp_addr(), {b1_rows[0], b1_rows[1], b1_rows[2], b1_rows[3]});
  const auto b0v = spadRows(b0.full_sp_addr(), {b0_rows[0], b0_rows[1], b0_rows[2], b0_rows[3]});
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

// ExCtrl reads math D from Accum and StCtrl reads a different Accum tile
// for a store. The two source tiles sit in different banks.
RsMemTestCase makeSharedAccumReadCase() {
  RsMemTestCase test{};
  test.name = "shared_accum_read";
  test.description = "ExCtrl D reads and Store reads traverse the shared Accum response path";
  test.max_cycles = 400;
  test.drain_cycles = 16;
  test.min_ex_accum_reads = kDim;
  test.min_store_accum_reads = kDim;

  constexpr std::uint64_t d_dram = 0x8000a000;
  constexpr std::uint64_t store_dram = 0x8000b000;
  constexpr std::uint64_t output_dram = 0x8000c000;
  const auto d_addr = makeAccAddr(0);
  const auto store_addr = makeAccAddr(8);
  const auto c_addr = makeAccAddr(12);
  const auto b_addr = makeSpAddr(4);
  const auto a_addr = makeSpAddr(8);

  std::array<MeshInputRow, kDim> a{};
  std::array<MeshInputRow, kDim> b{};
  std::array<MeshInputRow, kDim> d{};
  for (std::size_t r = 0; r < kDim; ++r) {
    b[r][r] = 1;
    MeshInputRow stored{};
    for (std::size_t col = 0; col < kDim; ++col) {
      a[r][col] = static_cast<Elem>(1 + r * kDim + col);
      d[r][col] = static_cast<Elem>(2 + r);
      stored[col] = static_cast<Elem>(40 + r * kDim + col);
    }
    test.spad_rows.push_back({b_addr + static_cast<std::uint32_t>(r), b[r]});
    test.spad_rows.push_back({a_addr + static_cast<std::uint32_t>(r), a[r]});
    test.dram_initial.push_back({d_dram + r * kDim, rowBytes(d[r])});
    test.dram_initial.push_back({store_dram + r * kDim, rowBytes(stored)});
    test.expected_dram.push_back({output_dram + r * kDim, rowBytes(stored)});
  }

  test.program = {
      loopCommand(SmeshFunct::Config,
                  packConfigLoadRs1(0, kDim, kMvinScaleIdentityBits, true), kDim),
      loopCommand(SmeshFunct::Mvin, d_dram,
                  packLocal(d_addr, MatrixShape{kDim, kDim})),
      loopCommand(SmeshFunct::Mvin, store_dram,
                  packLocal(store_addr, MatrixShape{kDim, kDim})),
      configExCmd(),
      loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store),
                  (std::uint64_t{kAccScaleIdentityBits} << 32) | kDim),
      preloadCmd(b_addr, c_addr),
      computeFlipCmd(a_addr, d_addr),
      loopCommand(SmeshFunct::Mvout, output_dram,
                  packLocal(store_addr, MatrixShape{kDim, kDim})),
  };
  test.expected_completion_tags = {1, 2, 3, 5, 6, 7};

  std::vector<MeshAccumRow> c;
  for (std::size_t r = 0; r < kDim; ++r) c.push_back(matmulRow(a, b, d, r));
  test.expected_results = {ExpectedAccumResult{c_addr, c}};
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
    const auto rs1 = packConfigLoadRs1(state, kDim, kMvinScaleIdentityBits, shrink);
    return command(SmeshFunct::Config, rs1, kDim);
  };
  test.program = {
      configLoad(0, false), configLoad(1, false), configLoad(2, true),
      configExCmd(),
      command(SmeshFunct::Config, packConfig(ConfigKind::Store),
              (std::uint64_t{kAccScaleIdentityBits} << 32) | kDim),
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

// One NHWC image (1x4 pixels, four channels), a 1x1 kernel producing two
// channels, and full-width bias. Compute the convolution directly from DRAM
// tensors; no worker command-generation code participates in this oracle.
RsMemTestCase makeLoopConvDmaCase() {
  RsMemTestCase test{};
  test.name = "loop_conv_dma";
  test.description = "LOOP_CONV_WS loads pixels/weights/bias, computes a 1x1 convolution, and stores NHWC output";
  test.expect_conv_release = true;
  test.max_cycles = 800;
  test.drain_cycles = 16;
  constexpr std::uint64_t input_base = 0x80014000;
  constexpr std::uint64_t weight_base = 0x80015000;
  constexpr std::uint64_t bias_base = 0x80016000;
  constexpr std::uint64_t output_base = 0x80017000;
  constexpr int input[4][4] = {{1, 2, 3, 4}, {2, 1, 0, 3}, {-1, 2, 1, 0}, {0, -2, 3, 1}};
  constexpr int weights[4][2] = {{1, 2}, {-1, 1}, {2, -1}, {1, 0}};
  constexpr Acc bias[2] = {3, -2};
  std::vector<std::uint8_t> bias_bytes;
  for (const auto value : bias) {
    for (unsigned byte = 0; byte < sizeof(Acc); ++byte)
      bias_bytes.push_back(static_cast<std::uint32_t>(value) >> (8 * byte));
  }
  test.dram_initial.push_back({bias_base, bias_bytes});
  std::vector<MeshAccumRow> output;
  for (unsigned pixel = 0; pixel < 4; ++pixel) {
    MeshInputRow pixels{};
    std::vector<std::uint8_t> bytes;
    for (unsigned channel = 0; channel < 4; ++channel) {
      pixels[channel] = input[pixel][channel];
      bytes.push_back(static_cast<std::uint8_t>(pixels[channel]));
    }
    test.dram_initial.push_back({input_base + pixel * 4, bytes});
    test.expected_loaded_spad_rows.push_back({makeSpAddr(pixel), pixels});
    MeshAccumRow result{};
    std::vector<std::uint8_t> stored;
    for (unsigned och = 0; och < 2; ++och) {
      result[och] = bias[och];
      for (unsigned channel = 0; channel < 4; ++channel)
        result[och] += input[pixel][channel] * weights[channel][och];
      stored.push_back(static_cast<std::uint8_t>(result[och]));
    }
    output.push_back(result);
    test.expected_dram.push_back({output_base + pixel * 2, stored});
  }
  for (unsigned channel = 0; channel < 4; ++channel) {
    MeshInputRow weight{};
    std::vector<std::uint8_t> bytes;
    for (unsigned och = 0; och < 2; ++och) {
      weight[och] = weights[channel][och];
      bytes.push_back(static_cast<std::uint8_t>(weight[och]));
    }
    test.dram_initial.push_back({weight_base + channel * 2, bytes});
    test.expected_loaded_spad_rows.push_back({makeSpAddr(kSpRows / 2 - 4 + channel), weight});
  }
  test.expected_results = {{makeAccAddr(0), output}};
  test.program = {
    configExCmd(),
    loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store),
                (std::uint64_t{kAccScaleIdentityBits} << 32) | 2),
    loopCommand(SmeshFunct::LoopConvWsConfig1, (2ull << 48) | (4ull << 32) | (1ull << 16) | 1,
                (1ull << 48) | (4ull << 32) | 1),
    loopCommand(SmeshFunct::LoopConvWsConfig2, 1ull << 48, (1ull << 48) | 2),
    loopCommand(SmeshFunct::LoopConvWsConfig3, (1ull << 48) | (1ull << 32) | (4ull << 16), 4),
    loopCommand(SmeshFunct::LoopConvWsConfig4, (1ull << 48) | 1,
                (4ull << 48) | (2ull << 32) | (2ull << 16) | 4),
    loopCommand(SmeshFunct::LoopConvWsConfig5, weight_base, output_base),
    loopCommand(SmeshFunct::LoopConvWsConfig6, bias_base, input_base),
    loopCommand(SmeshFunct::LoopConvWs, 0, 1),
  };
  // External CONFIG_EX then three loads, PRELOAD/COMPUTE, and STORE complete.
  // Load/Store CONFIGs retire on issue and do not reach controller completion.
  test.expected_completion_tags = {0, 3, 5, 7, 8, 9, 10};
  return test;
}

// Two independent images share weights, bias, and local input/weight storage.
// Both macro programs are issued by one driver without resetting Smesh.
RsMemTestCase makeLoopConvImagesCase() {
  RsMemTestCase test{};
  test.name = "loop_conv_images";
  test.description = "two distinct images run back-to-back with shared SPAD storage and separate DRAM outputs";
  test.expect_conv_release = true;
  test.max_cycles = 1400;
  test.drain_cycles = 16;
  constexpr std::uint64_t input_base = 0x80020000;
  constexpr std::uint64_t weight_base = 0x80021000;
  constexpr std::uint64_t bias_base = 0x80022000;
  constexpr std::uint64_t output_base = 0x80023000;
  constexpr unsigned image_spacing = 0x100;
  constexpr int input[2][4][4] = {
      {{1, 2, 3, 4}, {2, 1, 0, 3}, {-1, 2, 1, 0}, {0, -2, 3, 1}},
      {{-3, 1, 2, -2}, {4, -2, 1, 0}, {1, 0, -4, 3}, {-2, 3, -1, 2}},
  };
  constexpr int weights[4][2] = {{1, 2}, {-1, 1}, {2, -1}, {1, 0}};
  constexpr Acc bias[2] = {3, -2};
  std::vector<std::uint8_t> bias_bytes;
  for (const auto value : bias)
    for (unsigned byte = 0; byte < sizeof(Acc); ++byte)
      bias_bytes.push_back(static_cast<std::uint32_t>(value) >> (8 * byte));
  test.dram_initial.push_back({bias_base, bias_bytes});
  for (unsigned ich = 0; ich < 4; ++ich) {
    const auto weight = row({weights[ich][0], weights[ich][1]});
    test.dram_initial.push_back({weight_base + ich * 2,
        {static_cast<std::uint8_t>(weights[ich][0]), static_cast<std::uint8_t>(weights[ich][1])}});
    test.expected_loaded_spad_rows.push_back({makeSpAddr(kSpRows / 2 - 4 + ich), weight});
  }
  test.program = {
      configExCmd(),
      loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store),
                  (std::uint64_t{kAccScaleIdentityBits} << 32) | 2),
  };
  for (unsigned image = 0; image < 2; ++image) {
    const auto source = input_base + image * image_spacing;
    const auto destination = output_base + image * image_spacing;
    std::vector<MeshAccumRow> results;
    for (unsigned pixel = 0; pixel < 4; ++pixel) {
      MeshInputRow pixels{};
      std::vector<std::uint8_t> bytes;
      for (unsigned ich = 0; ich < 4; ++ich) {
        pixels[ich] = input[image][pixel][ich];
        bytes.push_back(static_cast<std::uint8_t>(pixels[ich]));
      }
      test.dram_initial.push_back({source + pixel * 4, bytes});
      // After both jobs, the common A region must contain only image 1.
      if (image == 1) test.expected_loaded_spad_rows.push_back({makeSpAddr(pixel), pixels});
      MeshAccumRow result{};
      std::vector<std::uint8_t> stored;
      for (unsigned och = 0; och < 2; ++och) {
        result[och] = bias[och];
        for (unsigned ich = 0; ich < 4; ++ich)
          result[och] += input[image][pixel][ich] * weights[ich][och];
        stored.push_back(static_cast<std::uint8_t>(result[och]));
      }
      results.push_back(result);
      test.expected_dram.push_back({destination + pixel * 2, stored});
    }
    test.expected_results.push_back({makeAccAddr(image * kAccRows / 2), results});
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWsConfig1,
        (2ull << 48) | (4ull << 32) | (1ull << 16) | 1, (1ull << 48) | (4ull << 32) | 1));
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWsConfig2, 1ull << 48, (1ull << 48) | 2));
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWsConfig3,
        (1ull << 48) | (1ull << 32) | (4ull << 16), 4));
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWsConfig4,
        (1ull << 48) | 1, (4ull << 48) | (2ull << 32) | (2ull << 16) | 4));
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWsConfig5, weight_base, destination));
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWsConfig6, bias_base, source));
    // Both jobs explicitly use A rows 0..3 and B rows 4..7, rather than
    // ping-ponging SPAD regions. RS hazards must protect the first image.
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWs, (1ull << 18) | (1ull << 16), 1));
  }
  // Each job emits three LOAD CONFIG/LOAD pairs, PRELOAD, COMPUTE, STORE.
  // The second bias CONFIG (tag 10) precedes the first STORE (tag 11).
  // CONFIG_STORE tag 1 and LOAD CONFIGs 2,4,6,10,13,15 retire on issue.
  test.expected_completion_tags = {0, 3, 5, 7, 8, 9, 11, 12, 14, 16, 17, 18, 19};
  return test;
}

// A 3x3 NHWC image, two input/output channels, and four distinct kernel
// positions. Tile into two output-row jobs: each uses six input SPAD rows
// and eight weight rows, fitting the existing 16-row scratchpad without overlap.
RsMemTestCase makeLoopConvKernel2Case() {
  RsMemTestCase test{};
  test.name = "loop_conv_kernel2";
  test.description = "2x2 kernel over a 3x3 image accumulates four kernel contributions and stores a 2x2 output";
  test.max_cycles = 1400;
  test.drain_cycles = 16;
  test.expect_conv_release = true;
  constexpr std::uint64_t input_base = 0x80018000;
  constexpr std::uint64_t weight_base = 0x80019000;
  constexpr std::uint64_t bias_base = 0x8001a000;
  constexpr std::uint64_t output_base = 0x8001b000;
  constexpr int input[3][3][2] = {
      {{1, 2}, {2, -1}, {0, 3}},
      {{-2, 1}, {3, 2}, {1, -2}},
      {{2, 0}, {-1, 3}, {4, 1}},
  };
  // [kernel row][kernel column][input channel][output channel]
  constexpr int weights[2][2][2][2] = {
      {{{1, -1}, {2, 1}}, {{-1, 2}, {1, -2}}},
      {{{2, 1}, {-1, 2}}, {{1, -2}, {2, 1}}},
  };
  constexpr Acc bias[2] = {3, -2};
  std::vector<std::uint8_t> bias_bytes;
  for (const auto value : bias)
    for (unsigned byte = 0; byte < sizeof(Acc); ++byte)
      bias_bytes.push_back(static_cast<std::uint32_t>(value) >> (8 * byte));
  test.dram_initial.push_back({bias_base, bias_bytes});
  for (unsigned row = 0; row < 3; ++row)
    for (unsigned col = 0; col < 3; ++col) {
      std::vector<std::uint8_t> bytes;
      for (unsigned ich = 0; ich < 2; ++ich) bytes.push_back(static_cast<std::uint8_t>(input[row][col][ich]));
      test.dram_initial.push_back({input_base + (row * 3 + col) * 2, bytes});
      // The second job leaves the last two image rows in the shared A region.
      if (row >= 1) {
        MeshInputRow pixel{};
        for (unsigned ich = 0; ich < 2; ++ich) pixel[ich] = input[row][col][ich];
        test.expected_loaded_spad_rows.push_back({makeSpAddr((row - 1) * 3 + col), pixel});
      }
    }
  const unsigned b_start = kSpRows - 8;
  for (unsigned kr = 0; kr < 2; ++kr)
    for (unsigned kc = 0; kc < 2; ++kc)
      for (unsigned ich = 0; ich < 2; ++ich) {
        std::vector<std::uint8_t> bytes;
        MeshInputRow weight{};
        for (unsigned och = 0; och < 2; ++och) {
          weight[och] = weights[kr][kc][ich][och];
          bytes.push_back(static_cast<std::uint8_t>(weight[och]));
        }
        const auto row = (kr * 2 + kc) * 2 + ich;
        test.dram_initial.push_back({weight_base + row * 2, bytes});
        test.expected_loaded_spad_rows.push_back({makeSpAddr(b_start + row), weight});
      }
  test.program = {
      configExCmd(),
      loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store),
                  (std::uint64_t{kAccScaleIdentityBits} << 32) | 2),
  };
  for (unsigned out_row = 0; out_row < 2; ++out_row) {
    std::vector<MeshAccumRow> results;
    for (unsigned out_col = 0; out_col < 2; ++out_col) {
      MeshAccumRow result{};
      std::vector<std::uint8_t> bytes;
      for (unsigned och = 0; och < 2; ++och) {
        result[och] = bias[och];
        // Direct convolution oracle, independent of all PCG address/counter code.
        for (unsigned kr = 0; kr < 2; ++kr)
          for (unsigned kc = 0; kc < 2; ++kc)
            for (unsigned ich = 0; ich < 2; ++ich)
              result[och] += input[out_row + kr][out_col + kc][ich] * weights[kr][kc][ich][och];
        bytes.push_back(static_cast<std::uint8_t>(result[och]));
      }
      results.push_back(result);
      test.expected_dram.push_back({output_base + (out_row * 2 + out_col) * 2, bytes});
    }
    test.expected_results.push_back({makeAccAddr(out_row * kAccRows / 2), results});
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWsConfig1,
        (2ull << 48) | (2ull << 32) | (3ull << 16) | 1, (1ull << 48) | (2ull << 32) | 2));
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWsConfig2, 2ull << 48, (1ull << 48) | 2));
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWsConfig3,
        (2ull << 48) | (2ull << 32) | (2ull << 16), 3));
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWsConfig4, (1ull << 48) | 1,
        (2ull << 48) | (2ull << 32) | (2ull << 16) | 2));
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWsConfig5, weight_base, output_base + out_row * 4));
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWsConfig6, bias_base, input_base + out_row * 6));
    // Explicit A region starts at zero; B ends at the full-SPAD boundary.
    // Both jobs share these regions; RS hazards protect the first job's reads.
    test.program.push_back(loopCommand(SmeshFunct::LoopConvWs, (1ull << 18) | (2ull << 16), 1));
  }
  // 40 primitive commands reach the RS: CONFIG_EX plus 32 load/execute/store
  // completions. CONFIG_STORE tag 1 and the three load CONFIGs in each job
  // (tags 2,4,9 and 20,23,28) retire on issue instead.
  for (unsigned tag = 0; tag < 40; ++tag) {
    if (tag == 1 || tag == 2 || tag == 4 || tag == 9 || tag == 20 || tag == 23 || tag == 28) continue;
    test.expected_completion_tags.push_back(tag);
  }
  return test;
}

// A horizontal 1x3 kernel processes each image row independently.
static RsMemTestCase makeLoopConvTemporal3RowsCase(unsigned input_rows, bool padded = false, unsigned pixel_repeats = 1) {
  RsMemTestCase test{};
  test.name        = padded 
      ? "loop_conv_temporal3_padded" 
      : input_rows == 1 
      ? "loop_conv_temporal3" 
      : "loop_conv_temporal3_2x6";
  test.description = padded
      ? "1x3 kernel with one zero at each end preserves the 1x6 two-channel output shape"
      : input_rows == 1
      ? "1x3 kernel over a 1x6 two-channel image produces four two-channel outputs"
      : "1x3 kernel over a 2x6 two-channel image produces eight two-channel outputs";
  if (pixel_repeats == 2) {
    test.name        += "_pixel2";
    test.description += "; pack two neighboring pixels into each SPAD row";
  }
  const unsigned pad         = padded ? 1 : 0;
  const unsigned output_cols = 4 + 2 * pad;
  const unsigned padded_cols = 6 + 2 * pad;
  test.max_cycles            = 1200;
  test.drain_cycles          = 16;
  test.expect_conv_release   = true;
  constexpr std::uint64_t input_base  = 0x80028000;
  constexpr std::uint64_t weight_base = 0x80029000;
  constexpr std::uint64_t bias_base   = 0x8002a000;
  constexpr std::uint64_t output_base = 0x8002b000;
  constexpr int input[2][6][2] = {
      {{1, 2}, {2, -1}, {0, 3}, {-2, 1}, {3, 2}, {1, -2}},
      {{-3, 1}, {4, 2}, {-1, -2}, {2, 0}, {0, 3}, {-2, 4}},
  };
  // [kernel time offset][input channel][output channel]; no kernel reversal.
  constexpr int weights[3][2][2] = {
      {{1, -1}, {2, 1}}, {{-1, 2}, {1, -2}}, {{2, 1}, {-1, 2}},
  };
  constexpr Acc bias[2] = {3, -2};
  std::vector<std::uint8_t> bias_bytes;
  for (const auto value : bias)
    for (unsigned byte = 0; byte < sizeof(Acc); ++byte)
      bias_bytes.push_back(static_cast<std::uint32_t>(value) >> (8 * byte));
  test.dram_initial.push_back({bias_base, bias_bytes});
  if (padded) {
    // A zero-fill load must not fetch this nonzero DRAM data at address zero.
    test.dram_initial.push_back({0, {91, 73, 55, 37, 19, 1, 83, 65}});
    for (unsigned position = 0; position < input_rows * padded_cols; ++position)
      test.spad_rows.push_back({makeSpAddr(position),
          pixel_repeats == 2 ? row({47, -31, 23, -17}) : row({47, -31})});
  }
  for (unsigned image_row = 0; image_row < input_rows; ++image_row) {
    if (padded) {
      const auto first = pixel_repeats == 2
          ? row({0, 0, input[image_row][0][0], input[image_row][0][1]}) : MeshInputRow{};
      // The final padding row's upper columns are outside every compute window;
      // its lower columns are zeroed, while the original poison above is retained.
      const auto last = pixel_repeats == 2 ? row({0, 0, 23, -17}) : MeshInputRow{};
      test.expected_loaded_spad_rows.push_back({makeSpAddr(image_row * padded_cols), first});
      test.expected_loaded_spad_rows.push_back({makeSpAddr((image_row + 1) * padded_cols - 1), last});
    }
    for (unsigned time = 0; time < 6; ++time) {
      MeshInputRow pixel{};
      std::vector<std::uint8_t> bytes;
      for (unsigned ich = 0; ich < 2; ++ich) {
        pixel[ich] = input[image_row][time][ich];
        bytes.push_back(static_cast<std::uint8_t>(pixel[ich]));
      }
      if (pixel_repeats == 2 && time + 1 < 6) {
        pixel[2] = input[image_row][time + 1][0];
        pixel[3] = input[image_row][time + 1][1];
      }
      const auto position = image_row * padded_cols + time + pad;
      test.dram_initial.push_back({input_base + (image_row * 6 + time) * 2, bytes});
      test.expected_loaded_spad_rows.push_back({makeSpAddr(position), pixel});
    }
  }
  const auto b_start = static_cast<std::uint32_t>(kSpRows - 6);
  for (unsigned tap = 0; tap < 3; ++tap)
    for (unsigned ich = 0; ich < 2; ++ich) {
      MeshInputRow weight{};
      std::vector<std::uint8_t> bytes;
      for (unsigned och = 0; och < 2; ++och) {
        weight[och] = weights[tap][ich][och];
        bytes.push_back(static_cast<std::uint8_t>(weight[och]));
      }
      const auto row = tap * 2 + ich;
      test.dram_initial.push_back({weight_base + row * 2, bytes});
      test.expected_loaded_spad_rows.push_back({makeSpAddr(b_start + row), weight});
    }
  std::vector<MeshAccumRow> results;
  for (unsigned image_row = 0; image_row < input_rows; ++image_row) {
    for (unsigned time = 0; time < output_cols; ++time) {
      MeshAccumRow result{};
      std::vector<std::uint8_t> bytes;
      for (unsigned och = 0; och < 2; ++och) {
        result[och] = bias[och];
        // Direct convolution stays within this image row, independent of worker commands.
        for (unsigned tap = 0; tap < 3; ++tap) {
          const int source = static_cast<int>(time + tap) - static_cast<int>(pad);
          if (source < 0 || source >= 6) continue;
          for (unsigned ich = 0; ich < 2; ++ich)
            result[och] += input[image_row][source][ich] * weights[tap][ich][och];
        }
        bytes.push_back(static_cast<std::uint8_t>(result[och]));
      }
      results.push_back(result);
      test.expected_dram.push_back({output_base + (image_row * output_cols + time) * 2, bytes});
    }
  }
  test.expected_results.push_back({makeAccAddr(0), results});
  test.program = {
      configExCmd(),
      loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store),
                  (std::uint64_t{kAccScaleIdentityBits} << 32) | 2),
      // One batch, two input/output channels; height is preserved by the 1x3 kernel.
      loopCommand(SmeshFunct::LoopConvWsConfig1,
                  (2ull << 48) | (2ull << 32) | (std::uint64_t{input_rows} << 16) | 1,
                  (std::uint64_t{pad} << 56) | (1ull << 48) |
                      (std::uint64_t{output_cols} << 32) | input_rows),
      // The normal weight layout uses kernel_dim as its kernel-row pitch.
      loopCommand(SmeshFunct::LoopConvWsConfig2, 3ull << 48, (1ull << 48) | 2),
      // Process one kernel row, three kernel columns, two channels; input width 6.
      loopCommand(SmeshFunct::LoopConvWsConfig3,
                  (1ull << 48) | (3ull << 32) | (2ull << 16) | pad,
                  (std::uint64_t{pad} << 48) | 6),
      loopCommand(SmeshFunct::LoopConvWsConfig4, (std::uint64_t{input_rows} << 48) | 1,
                  (2ull << 48) | (2ull << 32) | (2ull << 16) | output_cols),
      loopCommand(SmeshFunct::LoopConvWsConfig5, weight_base, output_base),
      loopCommand(SmeshFunct::LoopConvWsConfig6, bias_base, input_base),
      // Inputs include the horizontal zero borders; weights occupy the final six rows.
      loopCommand(SmeshFunct::LoopConvWs, (1ull << 18) | (2ull << 16) |
                  (std::uint64_t{pixel_repeats == 2 ? 2u : 0u} << 8), 1),
  };
  const unsigned output_blocks = input_rows * ((output_cols + kDim - 1) / kDim);
  const unsigned input_loads = input_rows * ((6 + kDim - 1) / kDim + 2 * pad);
  const unsigned tap_groups = pixel_repeats == 2 ? 2 : 3;
  // Each output block needs one bias load, a PRELOAD/COMPUTE pair per tap
  // group, and one store. The five CONFIGs and three weight loads are shared.
  // Bias, weight, and input workers issue in priority order. Load/Store CONFIGs
  // retire on issue; CONFIG_EX and every non-CONFIG command must complete.
  for (unsigned tag = 0; tag < 8 + input_loads + (2 + 2 * tap_groups) * output_blocks; ++tag) {
    if (tag == 1 || tag == 2 || tag == 3 + output_blocks || tag == 7 + output_blocks) continue;
    test.expected_completion_tags.push_back(tag);
  }
  if (pixel_repeats == 2) {
    // Golden grouped command stream: taps 0-1 use four inner elements; tap 2
    // uses two. Later output blocks reuse the weights of the same tap group.
    for (unsigned tap : {0u, 2u}) {
      const unsigned k = tap == 0 ? 4 : 2;
      for (unsigned start = 0; start < output_cols; start += kDim) {
        const unsigned rows = std::min<unsigned>(kDim, output_cols - start);
        test.expected_conv_execute.push_back(loopCommand(SmeshFunct::Preload,
            packLocal(start == 0 ? makeSpAddr(b_start + tap * 2).raw : 0xffffffffu, {k, 2}),
            packLocal(makeAccAddr(start, true), {rows, 2})));
        test.expected_conv_execute.push_back(loopCommand(
            start == 0 ? SmeshFunct::ComputeFlip : SmeshFunct::ComputeStay,
            packLocal(makeSpAddr(start + tap), {rows, k}),
            packLocal(0xffffffffu, {rows, 2})));
      }
    }
  }
  return test;
}

RsMemTestCase makeLoopConvTemporal3Case() {
  return makeLoopConvTemporal3RowsCase(1);
}

RsMemTestCase makeLoopConvTemporal3TwoRowsCase() {
  return makeLoopConvTemporal3RowsCase(2);
}

RsMemTestCase makeLoopConvTemporal3PaddedCase() {
  return makeLoopConvTemporal3RowsCase(1, true);
}

// Every output window touches padding: a 1x1 image and 2x2 kernel produce
// a 2x2 output with one pixel of padding on each side.
RsMemTestCase makeLoopConvPaddingCase() {
  RsMemTestCase test{};
  test.name = "loop_conv_padding";
  test.description = "all four padded edges are zero-filled before a biased 2x2 convolution";
  test.max_cycles = 1400;
  test.drain_cycles = 16;
  test.expect_conv_release = true;
  constexpr std::uint64_t input_base = 0x8001c000;
  constexpr std::uint64_t weight_base = 0x8001d000;
  constexpr std::uint64_t bias_base = 0x8001e000;
  constexpr std::uint64_t output_base = 0x8001f000;
  constexpr int pixel = -3;
  constexpr int weights[2][2][2] = {{{1, -2}, {3, 1}}, {{-1, 2}, {2, -3}}};
  constexpr Acc bias[2] = {5, -4};
  test.dram_initial.push_back({input_base, {static_cast<std::uint8_t>(pixel)}});
  std::vector<std::uint8_t> bias_bytes;
  for (const auto value : bias)
    for (unsigned byte = 0; byte < sizeof(Acc); ++byte)
      bias_bytes.push_back(static_cast<std::uint32_t>(value) >> (8 * byte));
  test.dram_initial.push_back({bias_base, bias_bytes});
  for (unsigned r = 0; r < 3; ++r)
    for (unsigned c = 0; c < 3; ++c) {
      // Poison the used lane so missing zero-fill cannot pass on reset contents.
      test.spad_rows.push_back({makeSpAddr(r * 3 + c), row({47})});
      test.expected_loaded_spad_rows.push_back({makeSpAddr(r * 3 + c),
                                               row({r == 1 && c == 1 ? pixel : 0})});
    }
  for (unsigned kr = 0; kr < 2; ++kr)
    for (unsigned kc = 0; kc < 2; ++kc) {
      const auto index = kr * 2 + kc;
      test.dram_initial.push_back({weight_base + index * 2,
          {static_cast<std::uint8_t>(weights[kr][kc][0]),
           static_cast<std::uint8_t>(weights[kr][kc][1])}});
      test.expected_loaded_spad_rows.push_back({makeSpAddr(kSpRows - 4 + index),
                                               row({weights[kr][kc][0], weights[kr][kc][1]})});
    }
  std::vector<MeshAccumRow> results;
  for (int r = 0; r < 2; ++r)
    for (int c = 0; c < 2; ++c) {
      MeshAccumRow result{};
      std::vector<std::uint8_t> bytes;
      for (unsigned och = 0; och < 2; ++och) {
        result[och] = bias[och];
        // Direct convolution: coordinates outside the actual image contribute zero.
        for (int kr = 0; kr < 2; ++kr)
          for (int kc = 0; kc < 2; ++kc)
            if (r + kr - 1 == 0 && c + kc - 1 == 0)
              result[och] += pixel * weights[kr][kc][och];
        bytes.push_back(static_cast<std::uint8_t>(result[och]));
      }
      results.push_back(result);
      test.expected_dram.push_back({output_base + (r * 2 + c) * 2, bytes});
    }
  test.expected_results = {{makeAccAddr(0), results}};
  test.program = {
      configExCmd(),
      loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store),
                  (std::uint64_t{kAccScaleIdentityBits} << 32) | 2),
      loopCommand(SmeshFunct::LoopConvWsConfig1, (2ull << 48) | (1ull << 32) | (1ull << 16) | 1,
                  (1ull << 56) | (1ull << 48) | (2ull << 32) | 2),
      loopCommand(SmeshFunct::LoopConvWsConfig2, 2ull << 48,
                  (1ull << 48) | (2ull << 32) | (2ull << 16) | 2),
      loopCommand(SmeshFunct::LoopConvWsConfig3, (2ull << 48) | (2ull << 32) | (1ull << 16) | 1,
                  (1ull << 48) | (1ull << 32) | (1ull << 24) | 1),
      loopCommand(SmeshFunct::LoopConvWsConfig4, (2ull << 48) | 1,
                  (1ull << 48) | (2ull << 32) | (2ull << 16) | 2),
      loopCommand(SmeshFunct::LoopConvWsConfig5, weight_base, output_base),
      loopCommand(SmeshFunct::LoopConvWsConfig6, bias_base, input_base),
      loopCommand(SmeshFunct::LoopConvWs, (1ull << 18) | (2ull << 16), 1),
  };
  // RS stream: external CONFIGs 0..1, bias CONFIG/loads 2..4,
  // weight CONFIG/loads 5..9, input CONFIG/loads 10..19,
  // eight PRELOAD/COMPUTE pairs 20..35, and two STOREs 36..37.
  // Load/Store CONFIGs retire on issue; CONFIG_EX reports completion.
  for (unsigned tag = 0; tag < 38; ++tag) {
    if (tag == 1 || tag == 2 || tag == 5 || tag == 10) continue;
    test.expected_completion_tags.push_back(tag);
  }
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
      loopCommand(SmeshFunct::Config, packConfigLoadRs1(0, kDim), bytes.size()),
      loopCommand(SmeshFunct::Mvin, dram_base,
                  packLocal(destination, MatrixShape{1, kDim})),
  };
  test.expected_completion_tags = {1};
  test.expected_results = {ExpectedAccumResult{destination, {values}}};
  return test;
}

// Load signed accumulator rows, then store scaled, ReLU, and original full-width
// values through the production finite scaler and DMA writer.
RsMemTestCase makeAccumScaleStoreCase() {
  RsMemTestCase test{};
  test.name = "accum_scale_store";
  test.description = "Finite Accum scaler stores rounded/clipped, ReLU, and preserved full-width rows";
  test.max_cycles = 600;
  test.min_store_accum_reads = 3 * kDim;

  constexpr std::uint64_t input_dram = 0x80010000;
  constexpr std::uint64_t scaled_dram = 0x80011000;
  constexpr std::uint64_t relu_dram = 0x80012000;
  constexpr std::uint64_t full_dram = 0x80013000;
  constexpr std::uint32_t half_scale = 0x3f000000u;
  const auto source = makeAccAddr(0);
  const auto full_source = makeAccAddr(0, false, true);
  constexpr Acc inputs[] = {3, 5, -3, -5, 255, -257, 256, -256};
  // Half-scale ties round to even; the remaining cases saturate to int8.
  constexpr int scaled[] = {2, 2, -2, -2, 127, -128, 127, -128};
  std::vector<MeshAccumRow> rows;
  for (std::size_t r = 0; r < kDim; ++r) {
    MeshAccumRow row{};
    std::vector<std::uint8_t> full_bytes;
    std::vector<std::uint8_t> scaled_bytes;
    std::vector<std::uint8_t> relu_bytes;
    for (std::size_t col = 0; col < kDim; ++col) {
      const auto index = (r * kDim + col) % 8;
      row[col] = inputs[index];
      const auto word = static_cast<std::uint32_t>(row[col]);
      for (std::size_t byte = 0; byte < sizeof(Acc); ++byte) {
        full_bytes.push_back(static_cast<std::uint8_t>(word >> (8 * byte)));
      }
      scaled_bytes.push_back(static_cast<std::uint8_t>(scaled[index]));
      relu_bytes.push_back(static_cast<std::uint8_t>(inputs[index] < 0 ? 0 : scaled[index]));
    }
    rows.push_back(row);
    test.dram_initial.push_back({input_dram + r * kDim * sizeof(Acc), full_bytes});
    test.expected_dram.push_back({scaled_dram + r * kDim, scaled_bytes});
    test.expected_dram.push_back({relu_dram + r * kDim, relu_bytes});
    test.expected_dram.push_back({full_dram + r * kDim * sizeof(Acc), full_bytes});
  }
  test.program = {
      loopCommand(SmeshFunct::Config, packConfigLoadRs1(0, kDim), kDim * sizeof(Acc)),
      loopCommand(SmeshFunct::Mvin, input_dram, packLocal(source, MatrixShape{kDim, kDim})),
      loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store),
                  (std::uint64_t{half_scale} << 32) | kDim),
      loopCommand(SmeshFunct::Mvout, scaled_dram, packLocal(source, MatrixShape{kDim, kDim})),
      loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store) | (1ull << 2),
                  (std::uint64_t{half_scale} << 32) | kDim),
      loopCommand(SmeshFunct::Mvout, relu_dram, packLocal(source, MatrixShape{kDim, kDim})),
      loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store) | (1ull << 2),
                  (std::uint64_t{half_scale} << 32) | (kDim * sizeof(Acc))),
      loopCommand(SmeshFunct::Mvout, full_dram, packLocal(full_source, MatrixShape{kDim, kDim})),
  };
  test.expected_completion_tags = {1, 3, 5, 7};
  test.expected_results = {ExpectedAccumResult{source, rows}};
  return test;
}

// Zero DRAM stride reads one row, then writes it to three local rows.
RsMemTestCase makeRepeatedLoadsCase() {
  RsMemTestCase test{};
  test.name = "repeated_loads";
  test.description = "Repeat and scale one Spad row; repeat one full-width Accum row";
  test.max_cycles = 180;

  constexpr std::uint64_t spad_base = 0x8000c000;
  constexpr std::uint64_t accum_base = 0x8000d000;
  constexpr std::uint32_t double_scale_bits = 0x40000000u; // binary32 2.0
  const auto spad_dest = makeSpAddr(0);
  const auto accum_dest = makeAccAddr(4);
  MeshInputRow spad_values{};
  MeshAccumRow accum_values{};
  std::vector<std::uint8_t> spad_bytes;
  std::vector<std::uint8_t> accum_bytes;
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    spad_bytes.push_back(static_cast<std::uint8_t>(lane + 1));
    spad_values[lane] = static_cast<Elem>(2 * (lane + 1));
    accum_values[lane] = static_cast<Acc>(0x12340000u + 257u * lane);
    const auto word = static_cast<std::uint32_t>(accum_values[lane]);
    for (unsigned byte = 0; byte < sizeof(Acc); ++byte) {
      accum_bytes.push_back(static_cast<std::uint8_t>(word >> (8 * byte)));
    }
  }
  test.dram_initial = {{spad_base, spad_bytes}, {accum_base, accum_bytes}};
  test.program = {
      loopCommand(SmeshFunct::Config, packConfigLoadRs1(0, kDim, double_scale_bits), 0),
      loopCommand(SmeshFunct::Mvin, spad_base,
                  packLocal(spad_dest, MatrixShape{3, kDim})),
      loopCommand(SmeshFunct::Mvin, accum_base,
                  packLocal(accum_dest, MatrixShape{3, kDim})),
  };
  for (std::uint32_t offset = 0; offset < 3; ++offset) {
    test.expected_loaded_spad_rows.push_back({spad_dest + offset, spad_values});
  }
  test.expected_results = {ExpectedAccumResult{
      accum_dest, {accum_values, accum_values, accum_values}}};
  test.expected_completion_tags = {1, 2};
  return test;
}

// ZeroWriter uses its own return path: masked writes must preserve untouched lanes,
// and only the final row of each zero-fill request acknowledges its whole byte count.
RsMemTestCase makeZeroWriterCase() {
  RsMemTestCase test{};
  test.name = "zero_writer";
  test.description = "Separate zero-fill branch with partial SPAD/Accum rows and ordinary DMA loads";
  test.max_cycles = 600;
  constexpr std::uint64_t acc_base = 0x80030000;
  constexpr std::uint64_t spad_base = 0x80031000;
  constexpr std::uint64_t saved_acc_base = 0x80032000;
  const auto spad_dest = makeSpAddr(kSpBankRows - 2);
  const auto acc_dest = makeAccAddr(8);
  MeshInputRow poison{};
  MeshInputRow normal{};
  MeshInputRow partial{};
  MeshAccumRow acc_poison{};
  MeshAccumRow acc_partial{};
  std::vector<std::uint8_t> acc_bytes;
  std::vector<std::uint8_t> spad_bytes;
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    poison[lane] = static_cast<Elem>(31 + lane);
    partial[lane] = lane < 2 ? 0 : poison[lane];
    normal[lane] = static_cast<Elem>(5 + lane);
    spad_bytes.push_back(static_cast<std::uint8_t>(normal[lane]));
    acc_poison[lane] = static_cast<Acc>(0x12340000 + lane);
    acc_partial[lane] = lane == 0 ? 0 : acc_poison[lane];
  }
  for (unsigned row = 0; row < 2; ++row) {
    for (auto value : acc_poison) {
      for (unsigned byte = 0; byte < sizeof(Acc); ++byte)
        acc_bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> (8 * byte)));
    }
  }
  test.spad_rows = {{spad_dest, poison}, {spad_dest + 1, poison}, {spad_dest + 2, poison}};
  test.dram_initial = {{acc_base, acc_bytes}, {spad_base, spad_bytes},
                       {0, std::vector<std::uint8_t>(kMemBeatBytes, 93)}};
  test.expected_dram = {{saved_acc_base,
      std::vector<std::uint8_t>(acc_bytes.begin(), acc_bytes.begin() + kDim * sizeof(Acc))}};
  test.program = {
      loopCommand(SmeshFunct::Config, packConfigLoadRs1(0, 2), 0),
      loopCommand(SmeshFunct::Mvin, acc_base, packLocal(acc_dest, MatrixShape{1, 2 * kDim})),
      // The store waits for the poison load; the later zero-fill waits for this
      // store's read. Issue order alone does not serialize the two load branches.
      loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store),
                  (std::uint64_t{kMvinScaleIdentityBits} << 32) | (kDim * sizeof(Acc))),
      loopCommand(SmeshFunct::Mvout, saved_acc_base,
                  packLocal(makeAccAddr(10, false, true), MatrixShape{1, kDim})),
      // NaN scaling must not affect the separate zero-fill path.
      loopCommand(SmeshFunct::Config, packConfigLoadRs1(0, 2, 0x7fc00000u), 0),
      loopCommand(SmeshFunct::Mvin, 0, packLocal(spad_dest, MatrixShape{1, kDim + 2})),
      loopCommand(SmeshFunct::Mvin, 0, packLocal(acc_dest, MatrixShape{1, kDim + 1})),
      loopCommand(SmeshFunct::Config, packConfigLoadRs1(0, 1), 0),
      loopCommand(SmeshFunct::Mvin, spad_base, packLocal(makeSpAddr(kSpRows - 1), MatrixShape{1, kDim})),
  };
  test.expected_loaded_spad_rows = {
      {spad_dest, MeshInputRow{}}, {spad_dest + 1, poison}, {spad_dest + 2, partial},
      {makeSpAddr(kSpRows - 1), normal}};
  test.expected_results = {{acc_dest, {MeshAccumRow{}}}, {acc_dest + 2, {acc_partial}}};
  test.expected_completion_tags = {1, 3, 5, 6, 8};
  return test;
}

// Shrunk accumulator loads use normal-width scaling before sign extension on write.
RsMemTestCase makeScaledShrinkLoadCase() {
  RsMemTestCase test{};
  test.name = "scaled_shrink_load";
  test.description = "Scale signed bytes into Accum with rounding, saturation, and zero scale";
  test.mvin_scale_latency = 4;
  test.max_cycles = 220;

  constexpr std::uint64_t half_source = 0x8000e000;
  constexpr std::uint64_t double_source = 0x8000f000;
  constexpr std::uint64_t zero_source = 0x80010000;
  constexpr std::uint32_t half_scale_bits = 0x3f000000u; // binary32 0.5
  constexpr std::uint32_t double_scale_bits = 0x40000000u; // binary32 2.0
  const auto destination = makeAccAddr(0);
  const std::uint8_t half_source_lanes[] = {0xfdu, 0xffu, 1u, 3u};
  const std::uint8_t double_source_lanes[] = {100u, 0x9cu, 127u, 0x80u};
  std::vector<std::uint8_t> half_bytes;
  std::vector<std::uint8_t> double_bytes;
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    half_bytes.push_back(half_source_lanes[lane % 4]);
    double_bytes.push_back(double_source_lanes[lane % 4]);
  }
  test.dram_initial = {{half_source, half_bytes}, {double_source, double_bytes},
                       {zero_source, half_bytes}};
  test.program = {
      loopCommand(SmeshFunct::Config,
                  packConfigLoadRs1(0, kDim, half_scale_bits, true), kDim),
      loopCommand(SmeshFunct::Mvin, half_source,
                  packLocal(destination, MatrixShape{1, kDim})),
      loopCommand(SmeshFunct::Config,
                  packConfigLoadRs1(0, kDim, double_scale_bits, true), kDim),
      loopCommand(SmeshFunct::Mvin, double_source,
                  packLocal(destination + 1, MatrixShape{1, kDim})),
      loopCommand(SmeshFunct::Config,
                  packConfigLoadRs1(0, kDim, 0, true), kDim),
      loopCommand(SmeshFunct::Mvin, zero_source,
                  packLocal(destination + 2, MatrixShape{1, kDim})),
  };
  MeshAccumRow half_expected{};
  MeshAccumRow double_expected{};
  const Acc half_lanes[] = {-2, 0, 0, 2};
  const Acc double_lanes[] = {127, -128, 127, -128};
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    half_expected[lane] = half_lanes[lane % 4];
    double_expected[lane] = double_lanes[lane % 4];
  }
  test.expected_results = {ExpectedAccumResult{
      destination, {half_expected, double_expected, MeshAccumRow{}}}};
  test.expected_completion_tags = {1, 3, 5};
  return test;
}

// Exercise full rows and partial lane masks in a separately compiled dim=8 system.
RsMemTestCase makeDim8LoadsCase() {
  RsMemTestCase test{};
  test.name = "dim8_loads";
  test.description = "Load full 8-element rows, then overwrite only six lanes";
  test.max_cycles = 280;

  constexpr std::uint64_t spad_base = 0x80008000;
  constexpr std::uint64_t accum_base = 0x80009000;
  constexpr std::uint64_t spad_partial_base = 0x8000a000;
  constexpr std::uint64_t accum_partial_base = 0x8000b000;
  const auto spad_dest = makeSpAddr(0);
  const auto accum_dest = makeAccAddr(0);
  MeshInputRow spad_row{};
  MeshAccumRow accum_row{};
  std::vector<std::uint8_t> spad_bytes;
  std::vector<std::uint8_t> accum_bytes;
  std::vector<std::uint8_t> spad_partial_bytes;
  std::vector<std::uint8_t> accum_partial_bytes;
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    spad_row[lane] = static_cast<Elem>(lane + 1);
    spad_bytes.push_back(static_cast<std::uint8_t>(spad_row[lane]));
    accum_row[lane] = static_cast<Acc>(0x12340000u + 257u * lane);
    const auto word = static_cast<std::uint32_t>(accum_row[lane]);
    for (unsigned byte = 0; byte < sizeof(Acc); ++byte) {
      accum_bytes.push_back(static_cast<std::uint8_t>(word >> (8 * byte)));
    }
    if (lane < 6) {
      spad_row[lane] = static_cast<Elem>(21 + lane);
      spad_partial_bytes.push_back(static_cast<std::uint8_t>(spad_row[lane]));
      accum_row[lane] = static_cast<Acc>(0x56780000u + 257u * lane);
      const auto partial_word = static_cast<std::uint32_t>(accum_row[lane]);
      for (unsigned byte = 0; byte < sizeof(Acc); ++byte) {
        accum_partial_bytes.push_back(static_cast<std::uint8_t>(partial_word >> (8 * byte)));
      }
    }
  }
  test.dram_initial = {{spad_base, spad_bytes}, {accum_base, accum_bytes},
                       {spad_partial_base, spad_partial_bytes},
                       {accum_partial_base, accum_partial_bytes}};
  test.program = {
      loopCommand(SmeshFunct::Config, packConfigLoadRs1(0, kDim),
                  kDim * sizeof(Acc)),
      loopCommand(SmeshFunct::Mvin, spad_base,
                  packLocal(spad_dest, MatrixShape{1, kDim})),
      loopCommand(SmeshFunct::Mvin, accum_base,
                  packLocal(accum_dest, MatrixShape{1, kDim})),
      loopCommand(SmeshFunct::Mvin, spad_partial_base,
                  packLocal(spad_dest, MatrixShape{1, 6})),
      loopCommand(SmeshFunct::Mvin, accum_partial_base,
                  packLocal(accum_dest, MatrixShape{1, 6})),
  };
  test.expected_loaded_spad_rows = {{spad_dest, spad_row}};
  test.expected_results = {ExpectedAccumResult{accum_dest, {accum_row}}};
  test.expected_completion_tags = {1, 2, 3, 4};
  return test;
}

std::vector<RsMemTestCase> rsMemTestCases() {
  auto tests = std::vector<RsMemTestCase>{
      makeBasicCase(), makeMulPreCase(), makeConcurrentBanksCase(),
      makeSameBankSerializesCase(), makeLoopWsCase(), makeLoopWsDmaCase(), makeLoopConvDmaCase(),
      makeLoopWsDmaI2Case(), makeLoopWsDmaK2Case(), makeFullWidthAccumLoadCase(),
      makeRepeatedLoadsCase(), makeScaledShrinkLoadCase(), makeAccumScaleStoreCase(), makeZeroWriterCase()};
  if (kDim == 4) tests.push_back(makeLoopConvKernel2Case());
  if (kDim == 4) tests.push_back(makeLoopConvTemporal3Case());
  if (kDim == 4) tests.push_back(makeLoopConvTemporal3TwoRowsCase());
  if (kDim == 4) tests.push_back(makeLoopConvTemporal3PaddedCase());
  if (kDim == 4 && kDefaultConfig.has_first_layer_optimizations) {
    tests.push_back(makeLoopConvTemporal3RowsCase(1, false, 2));
    tests.push_back(makeLoopConvTemporal3RowsCase(1, true, 2));
  }
  if (kDim == 4) tests.push_back(makeLoopConvPaddingCase());
  if (kDim == 4) tests.push_back(makeLoopConvImagesCase());
  if (kDim == 4) tests.push_back(makeSharedAccumReadCase());
  if (kDim == 8) tests.push_back(makeDim8LoadsCase());
  if (kMemBeatBytes == 4) {
    auto posted = makeLoopWsDmaCase();
    posted.name = "beat4_posted_loop_ws_dma";
    posted.description = "LOOP_WS at four-byte memory width with posted stores";
    posted.posted_writes = true;
    tests.push_back(std::move(posted));

    RsMemTestCase tail{};
    tail.name = "beat4_tail";
    tail.description = "Load and store three bytes through a four-byte memory interface";
    tail.posted_writes = true;
    tail.max_cycles = 250;
    tail.drain_cycles = 16;
    constexpr std::uint64_t source = 0x80008000;
    constexpr std::uint64_t dest = 0x80009000;
    const auto local = makeSpAddr(0);
    tail.dram_initial = {{source, {11, 22, 33}}};
    tail.program = {
        loopCommand(SmeshFunct::Config, packConfigLoadRs1(0, kDim), 3),
        loopCommand(SmeshFunct::Mvin, source,
                    packLocal(local, MatrixShape{1, 3})),
        loopCommand(SmeshFunct::Config, packConfig(ConfigKind::Store),
                    (std::uint64_t{kAccScaleIdentityBits} << 32) | 3),
        loopCommand(SmeshFunct::Mvout, dest,
                    packLocal(local, MatrixShape{1, 3})),
    };
    tail.expected_loaded_spad_rows = {{local, row({11, 22, 33, 0})}};
    tail.expected_dram = {{dest, {11, 22, 33}}};
    tail.expected_completion_tags = {1, 3};
    tests.push_back(std::move(tail));
  }
  return tests;
}

} // namespace tb
} // namespace smesh
