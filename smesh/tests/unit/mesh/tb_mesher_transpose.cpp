// **********************************************************************
// smesh/tests/unit/mesh/tb_mesher_transpose.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 23 2026
// Check the shared transposer's buffered inputs, columns, and WS numerical results.
#include "Mesher.hpp"

#include <descore/Parameter.hpp>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

using namespace smesh;
using Matrix = std::array<MeshInputRow, kDim>;

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, req_val);
  Output(ExCtrlMeshReq, req_bits);
  Output(bit, a_val);
  Output(ExCtrlMeshIn, a_bits);
  Output(bit, b_val);
  Output(ExCtrlMeshIn, b_bits);
  Output(bit, d_val);
  Output(ExCtrlMeshIn, d_bits);

  bool request_valid = false;
  ExCtrlMeshReq request{};
  bool a_valid = false, b_valid = false, d_valid = false;
  MeshInputRow a{}, b{}, d{};

  void update() {
    req_val = bit(request_valid);
    req_bits = request;
    a_val = bit(a_valid); a_bits = ExCtrlMeshIn{a};
    b_val = bit(b_valid); b_bits = ExCtrlMeshIn{b};
    d_val = bit(d_valid); d_bits = ExCtrlMeshIn{d};
  }
  void reset() override {
    request_valid = a_valid = b_valid = d_valid = false;
    request = {};
    a = {}; b = {}; d = {};
    req_val.reset(0); req_bits.reset(ExCtrlMeshReq{});
    a_val.reset(0); a_bits.reset(ExCtrlMeshIn{});
    b_val.reset(0); b_bits.reset(ExCtrlMeshIn{});
    d_val.reset(0); d_bits.reset(ExCtrlMeshIn{});
  }
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(req_val, req_bits, a_val, a_bits, b_val, b_bits, d_val, d_bits);
}

bool runCase(Driver& driver, Mesher& mesher, bool transpose_weights, bool pauses) {
  Sim::reset();
  Matrix a{}, b{}, reversed_b{};
  for (std::size_t row = 0; row < kDim; ++row) {
    for (std::size_t col = 0; col < kDim; ++col) {
      a[row][col] = static_cast<Elem>(int((row * 3 + col * 2) % 9) - 4);
      b[row][col] = static_cast<Elem>(int((row * 7 + col * 5) % 13) - 6);
    }
  }
  for (std::size_t row = 0; row < kDim; ++row) reversed_b[row] = b[kDim - 1 - row];

  bool ok = true;
  unsigned cycles = 0;
  MeshInputRow saved_a{}, saved_d{};
  std::vector<MeshInputRow> transposer_rows;
  std::vector<MeshAccumRow> results;
  auto step = [&]() {
    Sim::run();
    ++cycles;
    // Compare columns with a direct matrix transpose, independent of shift directions.
    MeshInputRow expected_column{};
    if (transposer_rows.size() >= kDim) {
      const auto matrix = transposer_rows.size() / kDim - 1;
      const auto column = transposer_rows.size() % kDim;
      for (std::size_t row = 0; row < kDim; ++row)
        expected_column[row] = transposer_rows[matrix * kDim + row][column];
    }
    if (*mesher.transposer_out_col_bits != expected_column) {
      std::printf("  cycle=%u: transposer column mismatch after %zu advances\n", cycles, transposer_rows.size());
      ok = false;
    }
    if (mesher.transposer_in_row_val == 1) {
      auto expected_input = transpose_weights ? saved_d : saved_a;
      if (transpose_weights) std::reverse(expected_input.begin(), expected_input.end());
      if (*mesher.transposer_in_row_bits != expected_input) {
        std::printf("  cycle=%u: transposer input differs from saved %s row\n", cycles, transpose_weights ? "D" : "A");
        ok = false;
      }
      transposer_rows.push_back(expected_input);
    }
    // Input handshakes update the operand buffers only for the following cycle.
    if (driver.a_valid && mesher.a_rdy == 1) saved_a = driver.a;
    if (driver.d_valid && mesher.d_rdy == 1) saved_d = driver.d;
    if (mesher.resp_val == 1 && mesher.resp_bits->tag.rs_tag_valid == 1 &&
        mesher.resp_bits->tag.rs_tag == 7) {
      results.push_back(mesher.resp_bits->data);
      ok &= mesher.resp_bits->last == bit(results.size() == kDim) &&
            mesher.resp_bits->total_rows == kDim;
    }
  };

  auto idle = [&]() {
    driver.a_valid = driver.b_valid = driver.d_valid = false;
    // Poison live inputs: advancing the mesh must use the previously saved rows.
    driver.a.fill(71); driver.b.fill(-63); driver.d.fill(59);
    step();
  };
  auto request = [&](bool propagate, bool tagged, unsigned flush = 0) {
    driver.request = {};
    driver.request.total_rows = kDim;
    driver.request.pe_control.dataflow = kExDataflowWS;
    driver.request.pe_control.propagate = bit(propagate);
    driver.request.a_transpose = bit(!transpose_weights);
    driver.request.bd_transpose = bit(transpose_weights);
    driver.request.flush = u8(flush);
    driver.request.tag.addr = SmeshLocalAddr{0xffffffffu};
    if (tagged) {
      driver.request.tag.rs_tag_valid = 1;
      driver.request.tag.rs_tag = 7;
      driver.request.tag.addr = makeAccAddr(0);
      driver.request.tag.rows = kDim;
      driver.request.tag.cols = kDim;
    }
    driver.request_valid = true;
    unsigned wait = 0;
    do { idle(); } while (mesher.req_rdy == 0 && ++wait < 100);
    ok &= mesher.req_rdy == 1;
    driver.request_valid = false;
  };
  auto rows = [&](const Matrix& as, const Matrix& ds) {
    for (std::size_t row = 0; row < kDim; ++row) {
      bool a_sent = false, b_sent = false, d_sent = false;
      unsigned wait = 0;
      do {
        driver.a_valid = !a_sent; driver.a = as[row];
        driver.b_valid = !b_sent; driver.b = {};
        driver.d_valid = !d_sent; driver.d = ds[row];
        step();
        a_sent |= mesher.a_rdy == 1;
        b_sent |= mesher.b_rdy == 1;
        d_sent |= mesher.d_rdy == 1;
      } while (!(a_sent && b_sent && d_sent) && ++wait < 100);
      ok &= a_sent && b_sent && d_sent;
      if (pauses) { idle(); idle(); }
    }
    driver.a_valid = driver.b_valid = driver.d_valid = false;
  };

  const Matrix zeros{};
  if (transpose_weights) {
    // First fill the transposer; its columns preload B-transpose on the next request.
    request(false, false);
    rows(zeros, reversed_b);
    request(false, true);
    rows(zeros, zeros);
    request(true, false);
    rows(a, zeros);
  } else {
    // Fill A's transposer while preloading B normally; the next request computes.
    request(false, true);
    rows(a, reversed_b);
    request(true, false);
    rows(zeros, zeros);
  }
  request(false, false, 2);
  for (std::size_t cycle = 0; cycle < 6 * kDim; ++cycle) idle();

  ok &= results.size() == kDim;
  for (std::size_t row = 0; row < results.size(); ++row) {
    if (row >= kDim) { ok = false; break; }
    for (std::size_t col = 0; col < kDim; ++col) {
      Acc expected = 0;
      for (std::size_t k = 0; k < kDim; ++k)
        expected += Acc(transpose_weights ? a[row][k] : a[k][row]) *
                    Acc(transpose_weights ? b[col][k] : b[k][col]);
      if (results[row][col] != expected) {
        std::printf("  row=%zu col=%zu expected=%d actual=%d\n", row, col, int(expected), int(results[row][col]));
        ok = false;
      }
    }
  }
  std::printf("[MESHER_TRANSPOSE] %s %s pauses=%u DIM=%zu rows=%zu cycles=%u\n",
              ok ? "PASS" : "FAIL", transpose_weights ? "A*B^T" : "A^T*B",
              unsigned(pauses), kDim, results.size(), cycles);
  return ok;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Clock clk;
  Driver driver("Driver");
  Mesher mesher("Mesher");
  driver.clk << clk; mesher.clk << clk;
  mesher.req_val << driver.req_val; mesher.req_bits << driver.req_bits;
  mesher.a_val << driver.a_val; mesher.a_bits << driver.a_bits;
  mesher.b_val << driver.b_val; mesher.b_bits << driver.b_bits;
  mesher.d_val << driver.d_val; mesher.d_bits << driver.d_bits;
  clk.generateClock();
  Sim::init();
  bool ok = true;
  for (bool transpose_weights : {false, true})
    for (bool pauses : {false, true}) ok &= runCase(driver, mesher, transpose_weights, pauses);
  descore::flushLog();
  return ok ? 0 : 1;
}
