// **********************************************************************
// smesh/src/tb_ex_ctrl_test_cases.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 17 2026
/*
Human-readable ExCtrl programs, memory images, and expected results.  Defines actual 
test scenarios using DSL vocabulary from tb_ex_ctrl_dsl.hpp.  
*/

#include "tb_ex_ctrl_test_cases.hpp"

namespace smesh {
namespace tb {

namespace {

SpadMatrixData rows0To3(std::string name, const MatrixRef& location) {
  return spadData(std::move(name), location, {
      inputRow({ 0,  1,  2,  3}),
      inputRow({ 4,  5,  6,  7}),
      inputRow({ 8,  9, 10, 11}),
      inputRow({12, 13, 14, 15}),
  });
}

SpadMatrixData rows4To7(std::string name, const MatrixRef& location) {
  return spadData(std::move(name), location, {
      inputRow({16, 17, 18, 19}),
      inputRow({20, 21, 22, 23}),
      inputRow({24, 25, 26, 27}),
      inputRow({28, 29, 30, 31}),
  });
}

SpadMatrixData rows8To11(std::string name, const MatrixRef& location) {
  return spadData(std::move(name), location, {
      inputRow({32, 33, 34, 35}),
      inputRow({36, 37, 38, 39}),
      inputRow({40, 41, 42, 43}),
      inputRow({44, 45, 46, 47}),
  });
}

SpadMatrixData rows12To15(std::string name, const MatrixRef& location) {
  return spadData(std::move(name), location, {
      inputRow({48, 49, 50, 51}),
      inputRow({52, 53, 54, 55}),
      inputRow({56, 57, 58, 59}),
      inputRow({60, 61, 62, 63}),
  });
}

} // namespace

// ********************* TEST CASES *********************

// Basic end-to-end test: preload B0, compute C0=A0*B0+D0,
// then reuse B0 for a standalone COMPUTE_STAY.
ExCtrlTestCase makeBasicFlipStayTest() {
  const auto d1 = spadMatrix(0);
  const auto b0 = spadMatrix(4); // that's math B
  const auto d0 = spadMatrix(4); // that's math D
  const auto a1 = spadMatrix(8);
  const auto a0 = spadMatrix(12);
  const auto c0 = accumMatrix(8);

  ExCtrlTestCase test{};
  test.id = 1;
  test.name = "basic";
  test.description = "CONFIG, initial PRELOAD, COMPUTE_FLIP, COMPUTE_STAY";

  test.program = {
      configEx(7),
      preload(8, b0, c0),
      computeFlip(9, a0, d0),
      computeStay(10, a1, d1),
  };

  test.spad = {
      rows0To3("D1", d1),
      rows4To7("B0 and D0", b0),
      rows8To11("A1", a1),
      rows12To15("A0", a0),
  };

  /*
  The test expects:
  - The configured scratchpad rows to be requested in the correct order.
  - Three operations to enter and pass through Mesher.
  - The first result, C0, to be written to accumulator row 8 onward.
  - All four command tags to complete exactly once.
  - The final standalone COMPUTE_STAY result not to be written because it has no following PRELOAD
    supplying destination metadata.
  */

  test.expected_spad_reads[0] = {0, 1, 2, 3}; // bank 0 should receive requests from global spad rows 0, 1, 2, 3 in that order
  test.expected_spad_reads[1] = {7, 6, 5, 4, 4, 5, 6, 7};
  test.expected_spad_reads[2] = {8, 9, 10, 11};
  test.expected_spad_reads[3] = {12, 13, 14, 15};

  // The initial PRELOAD supplies C0's destination. The trailing standalone
  // COMPUTE_STAY has no following PRELOAD, so its result is not written.
  test.expected_results = {
      matmulResult("C0", c0, a0, b0, d0),
  };
  test.expected_completions = {7, 8, 9, 10};
  test.expected_progress = {3, 3 * kDim, 3 * kDim};
  return test;
}

// Overlap test: preload B0, compute C0=A0*B0+D0 while preloading B1,
// then compute C1=A1*B1+D1 using the newly loaded weights.
ExCtrlTestCase makeComputePreloadOverlapTest() {
  const auto a0 = spadMatrix(0);
  const auto d0 = spadMatrix(4);
  const auto b1 = spadMatrix(8);
  const auto b0 = spadMatrix(12);
  const auto a1 = a0;
  const auto d1 = d0;
  const auto c0 = accumMatrix(0);
  const auto c1 = accumMatrix(8);

  ExCtrlTestCase test{};
  test.id = 2;
  test.name = "mul_pre";
  test.description = "COMPUTE and PRELOAD overlap with two result matrices";

  test.program = {
      configEx(7),
      preload(8, b0, c0),
      computeFlip(9, a0, d0),
      preload(10, b1, c1),
      computeFlip(11, a1, d1),
  };

  test.spad = {
      rows0To3("A0 and A1", a0),
      rows4To7("D0 and D1", d0),
      rows8To11("B1", b1),
      rows12To15("B0", b0),
  };

  /*
  The test expects:
  - The initial PRELOAD to load B0 and establish the C0 destination.
  - The first COMPUTE_FLIP and second PRELOAD to overlap.
  - The overlapping preload to load B1 and establish the C1 destination.
  - Both C0 and C1 to be written to their accumulator locations.
  - The expected scratchpad read sequences, mesh row counts, and accumulator data.
  - All five command tags to complete exactly once.

  */
  test.expected_spad_reads[0] = {0, 1, 2, 3, 0, 1, 2, 3};
  test.expected_spad_reads[1] = {4, 5, 6, 7, 4, 5, 6, 7};
  test.expected_spad_reads[2] = {11, 10, 9, 8};
  test.expected_spad_reads[3] = {15, 14, 13, 12};

  test.expected_results = {
      matmulResult("C0", c0, a0, b0, d0),
      matmulResult("C1", c1, a1, b1, d1),
  };
  test.expected_completions = {7, 8, 9, 10, 11};
  test.expected_progress = {3, 3 * kDim, 3 * kDim};
  return test;
}

std::vector<ExCtrlTestCase> exCtrlTestCases() {
  return {makeBasicFlipStayTest(), makeComputePreloadOverlapTest()};
}

} // namespace tb
} // namespace smesh
