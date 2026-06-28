// **********************************************************************
// smesh/src/tb_smesh_m2.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026
/*
Testbench that runs the same M1 command stream through Cascade FIFO ports.
*/
#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "SmeshCommand.hpp"
#include "SmeshCommandDriver.hpp"
#include "SmeshShell.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <vector>

using MatrixElem = std::array<std::array<smesh::Elem, smesh::kDim>, smesh::kDim>;
using MatrixAcc = std::array<std::array<smesh::Acc, smesh::kDim>, smesh::kDim>;

IntParameter(steps, 20, "Batch steps for tb_smesh_m2");

namespace {

constexpr std::uint64_t kAAddr = 0x1000;
constexpr std::uint64_t kBAddr = 0x2000;
constexpr std::uint64_t kCAddr = 0x3000;
constexpr std::size_t kBStridePadBytes = 3;

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

void writeElemMatrix(smesh::SmeshMemory& mem,
                     std::uint64_t base,
                     std::uint32_t stride,
                     const MatrixElem& matrix) {
  for (std::size_t r = 0; r < smesh::kDim; ++r) {
    for (std::size_t c = 0; c < smesh::kDim; ++c) {
      mem.writeElem(base + r * stride + c, matrix[r][c]);
    }
  }
}

bool checkAccMatrix(const smesh::SmeshMemory& mem,
                    std::uint64_t base,
                    const MatrixAcc& expected) {
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

std::vector<smesh::SmeshCmd> makeScript() {
  constexpr smesh::MatrixShape shape{smesh::kDim, smesh::kDim};
  constexpr std::uint32_t elem_stride = smesh::kDim * sizeof(smesh::Elem);
  constexpr std::uint32_t b_elem_stride = elem_stride + kBStridePadBytes;
  constexpr std::uint32_t acc_stride = smesh::kDim * sizeof(smesh::Acc);
  constexpr std::uint32_t a_spad_row = 0;
  constexpr std::uint32_t b_spad_row = smesh::kDim;
  constexpr std::uint32_t c_acc_row = 0;

  auto cmd = [](smesh::SmeshFunct funct, std::uint64_t rs1, std::uint64_t rs2) {
    return smesh::SmeshCmd{
        u32(static_cast<std::uint32_t>(funct)),
        u64(rs1),
        u64(rs2),
    };
  };

  return {
      cmd(smesh::SmeshFunct::Config, smesh::packConfig(smesh::ConfigKind::Load, 0, smesh::kDim), elem_stride),
      cmd(smesh::SmeshFunct::Config, smesh::packConfig(smesh::ConfigKind::Load, 1, smesh::kDim), b_elem_stride),
      cmd(smesh::SmeshFunct::Config, smesh::packConfig(smesh::ConfigKind::Load, 2, smesh::kDim), elem_stride),
      cmd(smesh::SmeshFunct::Config, smesh::packConfig(smesh::ConfigKind::Store), acc_stride),
      cmd(smesh::SmeshFunct::Config, smesh::packConfigExecuteRs1(1), smesh::packConfigExecuteRs2(1)),
      cmd(smesh::SmeshFunct::Mvin, kAAddr, smesh::packLocal(a_spad_row, shape)),
      cmd(smesh::SmeshFunct::Mvin2, kBAddr, smesh::packLocal(b_spad_row, shape)),
      cmd(smesh::SmeshFunct::Preload,
          smesh::packLocal(b_spad_row, shape),
          smesh::packLocal(c_acc_row, shape)),
      cmd(smesh::SmeshFunct::ComputeFlip, smesh::packLocal(a_spad_row, shape), 0),
      cmd(smesh::SmeshFunct::Mvout, kCAddr, smesh::packLocal(c_acc_row, shape)),
  };
}

bool runCase(const char* name,
             const MatrixElem& a,
             const MatrixElem& b,
             int max_steps) {
  smesh::SmeshShell shell("Smesh");
  smesh::SmeshCommandDriver driver("Driver");

  shell.cmd_in << driver.cmd_out;
  driver.resp_in << shell.resp_out;
  shell.m_req.sendToBitBucket();
  shell.m_resp.wireToZero();

  Clock clk;
  shell.clk << clk;
  driver.clk << clk;
  clk.generateClock();
  Sim::init();
  Sim::reset();

  auto& mem = shell.memory();
  constexpr std::uint32_t elem_stride = smesh::kDim * sizeof(smesh::Elem);
  constexpr std::uint32_t b_elem_stride = elem_stride + kBStridePadBytes;
  writeElemMatrix(mem, kAAddr, elem_stride, a);
  writeElemMatrix(mem, kBAddr, b_elem_stride, b);
  driver.setScript(makeScript());

  for (int i = 0; i < max_steps && !driver.done(); ++i) {
    Sim::run();
  }

  const auto expected = referenceMatmul(a, b);
  const bool ok = driver.done() && !driver.failed() &&
                  checkAccMatrix(shell.memory(), kCAddr, expected);
  std::printf("[SMESH_M2] %s %s\n", ok ? "PASS" : "FAIL", name);
  return ok;
}

} // namespace

int main(int argc, char* argv[]) {
  try {
    descore::parseTraces(argc, argv);
    Parameter::parseCommandLine(argc, argv);
    Sim::parseDumps(argc, argv);

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

    const bool ok_identity = runCase("identity", a, identity, static_cast<int>(steps));
    const bool ok_matmul = runCase("matmul", a, b, static_cast<int>(steps));
    return (ok_identity && ok_matmul) ? 0 : 1;
  } catch (const std::exception& e) {
    std::printf("[SMESH_M2] FAIL exception: %s\n", e.what());
    return 1;
  }
}
