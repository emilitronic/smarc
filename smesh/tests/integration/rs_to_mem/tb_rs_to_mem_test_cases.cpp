// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_test_cases.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 19 2026
/*
rs_to_mem test scenarios. Each program is issued as raw SmeshCmds through
the real SmeshCmdQueue -> SmeshUnrolledCmdQueue -> SmeshRS -> ExCtrl path
(see tb_rs_to_mem_harness.hpp), so completion tags are assigned by the
real SmeshRS at allocation time rather than hand-picked -- since our
programs are issued one command per cycle and RS allocates strictly in
arrival order, tags come out sequentially as 0, 1, 2, ... in program
order. expected_completion_order below is that sequence, permuted into
the order ExCtrl actually reports completions (which is not program
order -- see each scenario's comment).
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
// by RS as tags 0,1,2,3 in that order. Completion order is NOT program
// order: CONFIG completes immediately (tag 0), then COMPUTE_FLIP (tag 2)
// once its mesh result is written back, then COMPUTE_STAY (tag 3, no
// writeback needed since it has no destination), and PRELOAD (tag 1) last
// since its own completion is only reported once its accompanying
// COMPUTE's result has been written -- matching what ex_ctrl's suite
// already established for this exact scenario.
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

// "mul_pre": exercises the overlapping COMPUTE+PRELOAD path -- the second
// PRELOAD is issued right after the first COMPUTE_FLIP, so ExCtrl accepts
// them together as a single "mul_pre" operation (weights for the second
// matmul load into the array's other stationary-weight register while the
// first matmul is still computing). Produces two independent results.
// Real-RS equivalent of ex_ctrl's makeComputePreloadOverlapTest().
//
// Program order: CONFIG, PRELOAD0, COMPUTE_FLIP0, PRELOAD1, COMPUTE_FLIP1
// -- tags 0..4. Completion order: CONFIG(0), then both COMPUTE_FLIPs in
// program order (2, 4) once their results land, then both PRELOADs (1, 3)
// once their accompanying COMPUTEs have completed.
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

} // namespace tb
} // namespace smesh
