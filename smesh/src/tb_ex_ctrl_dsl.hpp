// **********************************************************************
// smesh/src/tb_ex_ctrl_dsl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 17 2026
/*
Typed, human-readable descriptions for ExCtrl end-to-end tests.  Helper functions
that let test scenarios described ExCtrl operations in HW-oriented terms.  Defines 
readable C++ vocabulary for commands, matrices, memory initialization, and 
expectations. It contains no simulated component.

For example instead of writing:

  issue(tag, SmeshFunct::Preload,
        packLocal(makeSpAddr(b_base), {kDim, kDim}),
        packLocal(makeAccAddr(c_base), {kDim, kDim}))

  a scenario can write:

  const auto b0 = spadMatrix(b_base);
  const auto c0 = accumMatrix(c_base);

  preload(tag, b0, c0)
*/
#pragma once

#include "SmeshCommand.hpp"
#include "SmeshPorts.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace smesh {
namespace tb {

enum class LocalMemory {
  Spad,
  Accum,
};

// A named matrix location used by command and expectation descriptions
struct MatrixRef {
  LocalMemory memory = LocalMemory::Spad;
  std::uint32_t base = 0;
  MatrixShape shape{kDim, kDim};
};

inline MatrixRef spadMatrix(std::uint32_t base,
                            std::size_t rows = kDim,
                            std::size_t cols = kDim) {
  return MatrixRef{LocalMemory::Spad, base, MatrixShape{rows, cols}};
}

inline MatrixRef accumMatrix(std::uint32_t base,
                             std::size_t rows = kDim,
                             std::size_t cols = kDim) {
  return MatrixRef{LocalMemory::Accum, base, MatrixShape{rows, cols}};
}
// A single row of input data for the mesh, used in spad or accum initialization
inline SmeshLocalAddr localAddress(const MatrixRef& matrix) {
  return matrix.memory == LocalMemory::Spad
             ? makeSpAddr(matrix.base)
             : makeAccAddr(matrix.base);
}

inline std::uint64_t packedMatrix(const MatrixRef& matrix) {
  return packLocal(localAddress(matrix), matrix.shape);
}

// Readable CONFIG_EX settings; the helper below performs the ISA packing
struct ConfigExSettings {
  std::uint32_t a_stride = 1;
  std::uint32_t c_stride = 1;
  std::uint32_t dataflow = kExDataflowWS;
  std::uint32_t activation = 0;
  std::uint32_t acc_scale = 0;
  std::uint32_t in_shift = 0;
  std::uint32_t relu6_shift = 0;
  bool a_transpose = false;
  bool b_transpose = false;
  bool set_only_strides = false;
};

inline SmeshIssue issue(SmeshRsTag tag, SmeshFunct funct,
                        std::uint64_t rs1 = 0,
                        std::uint64_t rs2 = 0,
                        bool tag_valid = true) {
  SmeshIssue result{};
  result.rs_tag_valid = bit(tag_valid);
  result.rs_tag = tag;
  result.cmd.funct = static_cast<std::uint32_t>(funct);
  result.cmd.rs1 = rs1;
  result.cmd.rs2 = rs2;
  return result;
}

inline SmeshIssue configEx(SmeshRsTag tag,
                           const ConfigExSettings& settings = {}) {
  return issue(
      tag, SmeshFunct::Config,
      packConfigExRs1(settings.a_stride, settings.a_transpose,
                      settings.b_transpose, settings.dataflow,
                      settings.set_only_strides, settings.activation,
                      settings.acc_scale),
      packConfigExRs2(settings.c_stride, settings.in_shift,
                      settings.relu6_shift));
}

inline SmeshIssue preload(SmeshRsTag tag, const MatrixRef& weights,
                          const MatrixRef& output) {
  return issue(tag, SmeshFunct::Preload,
               packedMatrix(weights), packedMatrix(output));
}

inline SmeshIssue computeFlip(SmeshRsTag tag, const MatrixRef& input,
                              const MatrixRef& addend) {
  return issue(tag, SmeshFunct::ComputeFlip,
               packedMatrix(input), packedMatrix(addend));
}

inline SmeshIssue computeStay(SmeshRsTag tag, const MatrixRef& input,
                              const MatrixRef& addend) {
  return issue(tag, SmeshFunct::ComputeStay,
               packedMatrix(input), packedMatrix(addend));
}

// One explicitly initialized matrix in the test scratchpad image.
struct SpadMatrixData {
  std::string name;
  MatrixRef location{};
  std::vector<MeshInputRow> rows;
};
// Convert readable list of int vals into MeshInputRow
inline MeshInputRow inputRow(std::initializer_list<int> values) {
  MeshInputRow row{};
  assert_always(values.size() <= row.size(),
                "test row has %zu values but mesh row has %zu lanes",
                values.size(), row.size());
  std::size_t lane = 0;
  for (const int value : values) {
    row[lane++] = static_cast<Elem>(value);
  }
  return row;
}
// Describes one matrix's initial contents and location in test spad
inline SpadMatrixData spadData(std::string name, const MatrixRef& location,
                               std::initializer_list<MeshInputRow> rows) {
  assert_always(location.memory == LocalMemory::Spad,
                "SPAD test data must use a scratchpad location");
  return SpadMatrixData{std::move(name), location,
                        std::vector<MeshInputRow>(rows)};
}

// One expected C=A*B+D result and its accumulator destination.
struct ExpectedMatmul {
  std::string name;
  MatrixRef output{};
  MatrixRef input{};
  MatrixRef weights{};
  MatrixRef addend{};
};

inline ExpectedMatmul matmulResult(std::string name,
                                   const MatrixRef& output,
                                   const MatrixRef& input,
                                   const MatrixRef& weights,
                                   const MatrixRef& addend) {
  return ExpectedMatmul{std::move(name), output, input, weights, addend};
}

struct ExCtrlExpectedProgress {
  std::size_t mesh_requests = 0;
  std::size_t mesh_input_rows = 0;
  std::size_t mesh_output_rows = 0;
};

// Complete declarative description of one independent ExCtrl test.
struct ExCtrlTestCase {
  int id = 0; // test ID for CL selection
  std::string name; // unique test name for selection & reporting
  std::string description; // human-readable summary printed by, e.g., -list_tests=1
  std::vector<SmeshIssue> program; // ordered RS command stream
  std::vector<SpadMatrixData> spad; // initial spad contents
  std::array<std::vector<std::uint32_t>, kSpBanks> expected_spad_reads{}; // expected sequence of full spad row addrs requested from ea. physical bank
  std::vector<ExpectedMatmul> expected_results; // matrix results expected to be written into accum
  std::vector<SmeshRsTag> expected_completions; // RS tags that must be reported as completed (ea. list tag is expected exactly once), completion order not checked
  ExCtrlExpectedProgress expected_progress{}; // expected high-level Mesher activity; checks how many op reqs and row-beats pass through Mesher
  int max_cycles = 96; // maximum number of cycles to run the test
  int drain_cycles = 4; // number of additional cycles to run after all expected activity has appeared
};

} // namespace tb
} // namespace smesh
