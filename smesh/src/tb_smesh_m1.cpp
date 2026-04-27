// **********************************************************************
// smesh/src/tb_smesh_m1.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Apr 27 2026
/*
Testbench to exercise the command API.  Runs the same matmul test as M0, but through the command API instead of direct state manipulation.  This is a more realistic test of how software will interact with the device, and also serves as a sanity check for the command encoding/decoding logic in SmeshDevice::executeCustom().
*/

#include "SmeshCommand.hpp" // for command encoding helpers
#include "SmeshDevice.hpp"  // for SmeshDevice and SmeshMemory

#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>

namespace {

using MatrixElem = std::array<std::array<smesh::Elem, smesh::kDim>, smesh::kDim>;
using MatrixAcc = std::array<std::array<smesh::Acc, smesh::kDim>, smesh::kDim>;

constexpr std::uint64_t kAAddr = 0x1000;
constexpr std::uint64_t kBAddr = 0x2000;
constexpr std::uint64_t kCAddr = 0x3000;

MatrixAcc referenceMatmul(const MatrixElem& a, const MatrixElem& b) {
  MatrixAcc out{};
  for (std::size_t r = 0; r < smesh::kDim; ++r) {
    for (std::size_t c = 0; c < smesh::kDim; ++c) {
      smesh::Acc sum = 0;
      for (std::size_t k = 0; k < smesh::kDim; ++k) {
        sum += static_cast<smesh::Acc>(a[r][k]) * b[k][c];
      }
      out[r][c] = sum;
    }
  }
  return out;
}

void writeElemMatrix(smesh::SmeshMemory& mem, std::uint64_t base, const MatrixElem& matrix) {
  for (std::size_t r = 0; r < smesh::kDim; ++r) {
    for (std::size_t c = 0; c < smesh::kDim; ++c) {
      mem.writeElem(base + r * smesh::kDim + c, matrix[r][c]);
    }
  }
}

bool checkAccMatrix(const smesh::SmeshMemory& mem, std::uint64_t base, const MatrixAcc& expected) {
  bool ok = true;
  const std::uint32_t stride = smesh::kDim * sizeof(smesh::Acc);
  for (std::size_t r = 0; r < smesh::kDim; ++r) {
    for (std::size_t c = 0; c < smesh::kDim; ++c) {
      const auto got = mem.readAcc(base + r * stride + c * sizeof(smesh::Acc));
      if (got != expected[r][c]) {
        std::printf("MISMATCH r=%zu c=%zu got=%d expected=%d\n",
                    r, c, got, expected[r][c]);
        ok = false;
      }
    }
  }
  return ok;
}

bool runCase(const char* name, const MatrixElem& a, const MatrixElem& b) {
  smesh::SmeshMemory mem;
  smesh::SmeshDevice dev;
  dev.reset();

  writeElemMatrix(mem, kAAddr, a);
  writeElemMatrix(mem, kBAddr, b);

  // Set up command parameters
  constexpr smesh::MatrixShape shape{smesh::kDim, smesh::kDim};
  constexpr std::uint32_t elem_stride = smesh::kDim * sizeof(smesh::Elem); // bytes between consecutive source rows in DRAM when doing mvin
  constexpr std::uint32_t acc_stride = smesh::kDim * sizeof(smesh::Acc); // bytes between consecutive destination rows in DRAM when doing mvout
  constexpr std::uint32_t a_spad_row = 0;
  constexpr std::uint32_t b_spad_row = smesh::kDim;
  constexpr std::uint32_t c_acc_row = 0;

  // sending decoded low-level commands
  dev.executeCustom(mem, smesh::SmeshFunct::Config,
                    smesh::packConfig(smesh::ConfigKind::Load), elem_stride); // set load stride
  dev.executeCustom(mem, smesh::SmeshFunct::Config,
                    smesh::packConfig(smesh::ConfigKind::Store), acc_stride); // set store stride
  dev.executeCustom(mem, smesh::SmeshFunct::Config,
                    smesh::packConfig(smesh::ConfigKind::Execute), 0);

  dev.executeCustom(mem, smesh::SmeshFunct::Mvin, kAAddr,
                    smesh::packLocal(a_spad_row, shape)); // move A from DRAM to SP
  dev.executeCustom(mem, smesh::SmeshFunct::Mvin, kBAddr,
                    smesh::packLocal(b_spad_row, shape)); // move B from DRAM to SP
  dev.executeCustom(mem, smesh::SmeshFunct::Preload,
                    smesh::packLocal(b_spad_row, shape),
                    smesh::packLocal(c_acc_row, shape));  // preload B into PE and set up C accum row
  dev.executeCustom(mem, smesh::SmeshFunct::ComputePreloaded,
                    smesh::packLocal(a_spad_row, shape), 0);  // compute A*B using preloaded B
  dev.executeCustom(mem, smesh::SmeshFunct::Mvout, kCAddr,
                    smesh::packLocal(c_acc_row, shape));  // move C from acc to DRAM

  const auto expected = referenceMatmul(a, b);
  const bool ok = checkAccMatrix(mem, kCAddr, expected);
  std::printf("[SMESH_M1] %s %s\n", ok ? "PASS" : "FAIL", name);
  return ok;
}

} // namespace

int main() {
  try {
    const MatrixElem a{{
        {{1, 2, 3, 4}},
        {{5, 6, 7, 8}},
        {{1, 0, 1, 0}},
        {{0, 1, 0, 1}},
    }};

    const MatrixElem identity{{
        {{1, 0, 0, 0}},
        {{0, 1, 0, 0}},
        {{0, 0, 1, 0}},
        {{0, 0, 0, 1}},
    }};

    const MatrixElem b{{
        {{1, 2, 0, -1}},
        {{0, 1, 3, 2}},
        {{4, 0, 1, 1}},
        {{2, -2, 1, 0}},
    }};

    const bool ok_identity = runCase("identity", a, identity);
    const bool ok_matmul = runCase("matmul", a, b);
    return (ok_identity && ok_matmul) ? 0 : 1;
  } catch (const std::exception& e) {
    std::printf("[SMESH_M1] FAIL exception: %s\n", e.what());
    return 1;
  }
}
