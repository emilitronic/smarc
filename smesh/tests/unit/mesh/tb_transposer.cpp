// **********************************************************************
// smesh/tests/unit/mesh/tb_transposer.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 23 2026
// Check Original's always-output contract with independent matrix-column expectations.
#include "Transposer.hpp"

#include <descore/Parameter.hpp>

#include <cstdio>
#include <vector>

namespace {

using namespace smesh;

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, row_val);
  Output(MeshInputRow, row_bits);
  Output(bit, col_rdy);

  // Testbench stimulus, changed between Sim::run() calls.
  bool valid = false;
  bool ready = true;
  MeshInputRow data{};

  void update() {
    row_val = bit(valid);
    row_bits = data;
    col_rdy = bit(ready);
  }
  void reset() override {
    valid = false;
    ready = true;
    data = {};
    row_val.reset(0);
    row_bits.reset(MeshInputRow{});
    col_rdy.reset(1);
  }
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(row_val, row_bits, col_rdy);
}

MeshInputRow inputRow(unsigned matrix, std::size_t row) {
  MeshInputRow values{};
  for (std::size_t col = 0; col < kDim; ++col) {
    // Signed, asymmetric values distinguish rows, columns, and consecutive matrices.
    values[col] = static_cast<Elem>(static_cast<int>((matrix * 53 + row * 11 + col * 3) % 251) - 125);
  }
  return values;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  Clock clk;
  Driver driver("Driver");
  Transposer transposer("Transposer");
  driver.clk << clk;
  transposer.clk << clk;
  transposer.in_row_val << driver.row_val;
  transposer.in_row_bits << driver.row_bits;
  transposer.out_col_rdy << driver.col_rdy;
  clk.generateClock();
  Sim::init();
  Sim::reset();

  bool ok = true;
  unsigned cycles = 0;
  std::vector<MeshInputRow> accepted;
  auto step = [&](bool valid, const MeshInputRow& data, bool ready = true) {
    driver.valid = valid;
    driver.data = data;
    driver.ready = ready;
    Sim::run();
    ++cycles;

    // This cycle's input changes D; Q contains the rows accepted in earlier cycles.
    MeshInputRow expected{};
    if (accepted.size() >= kDim) {
      const auto matrix = accepted.size() / kDim - 1;
      const auto column = accepted.size() % kDim;
      for (std::size_t row = 0; row < kDim; ++row) {
        expected[row] = accepted[matrix * kDim + row][column];
      }
    }
    const auto actual = *transposer.out_col_bits;
    if (actual != expected || transposer.in_row_rdy != 1 || transposer.out_col_val != 1) {
      std::printf("  cycle=%u prior_rows=%zu valid=%u ready=%u: unexpected output\n",
                  cycles, accepted.size(), unsigned(valid), unsigned(ready));
      for (std::size_t element = 0; element < kDim; ++element) {
        std::printf("    element=%zu expected=%d actual=%d\n", element,
                    int(expected[element]), int(actual[element]));
      }
      ok = false;
    }
    if (valid) accepted.push_back(data);
  };

  auto stream = [&](bool pauses) {
    for (unsigned matrix = 0; matrix < 3; ++matrix) {
      for (std::size_t row = 0; row < kDim; ++row) {
        if (pauses) {
          // Changing input data during a pause must not shift the grid or counter.
          step(false, inputRow(8, row), false);
          step(false, inputRow(9, row));
        }
        // Even with output-ready low, Original advances on every valid input row.
        step(true, inputRow(matrix, row), row % 2 == 0);
      }
    }
    // An extra matrix's worth of input advances drains all columns of matrix 2.
    for (std::size_t row = 0; row < kDim; ++row) step(true, MeshInputRow{});
    step(false, MeshInputRow{});
    step(false, inputRow(7, 0), false);
  };

  step(false, MeshInputRow{});
  stream(false);
  Sim::reset();
  accepted.clear();
  step(false, MeshInputRow{});
  stream(true);

  // Reset while partway through the second direction, then start a fresh stream.
  Sim::reset();
  accepted.clear();
  for (std::size_t row = 0; row < kDim + 1; ++row) step(true, inputRow(5, row));
  Sim::reset();
  accepted.clear();
  step(false, MeshInputRow{});
  stream(false);

  std::printf("[TRANSPOSER] %s DIM=%zu columns, overlap, pauses, ignored ready, flush, reset\n",
              ok ? "PASS" : "FAIL", kDim);
  descore::flushLog();
  return ok ? 0 : 1;
}
