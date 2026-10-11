// **********************************************************************
// smfft/tests/tb_sdf_twiddle.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Checks the generated twiddle tables against the canonical files in
tests/vectors/ exactly. No Cascade component or simulation is involved.

The canonical files are the arbiter: doc/fixed-point-spec.md 5.1 permits a
C++ model to compute the constants itself only if the result matches them.
So this test also guards against the real risk here, which is the host libm
disagreeing with Python's in the last integer.

Each file holds the whole chain for its N: one block of rows per stage delay,
k = 1 .. d-1, with k = 0 omitted. The row count is checked as well as the
values, so a truncated or partial file cannot pass by matching only what it
happens to contain.
*/

#include "SdfTwiddle.hpp"

#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace smfft;

struct Row {
  std::size_t d = 0;
  std::size_t k = 0;
  std::int64_t re = 0;
  std::int64_t im = 0;
};

bool readCanonical(const std::string& path, std::vector<Row>& rows) {
  std::ifstream f(path);
  if (!f) {
    std::printf("  cannot open %s\n", path.c_str());
    return false;
  }
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::istringstream s(line);
    Row r{};
    if (!(s >> r.d >> r.k >> r.re >> r.im)) {
      std::printf("  malformed line in %s: %s\n", path.c_str(), line.c_str());
      return false;
    }
    rows.push_back(r);
  }
  return true;
}

// Every row must match, and every k = 1..d-1 of every delay present in the
// file must be covered by exactly one row.
bool checkFile(const std::string& path, std::size_t B_w) {
  std::vector<Row> rows;
  if (!readCanonical(path, rows)) {
    return false;
  }
  if (rows.empty()) {
    std::printf("  %s contained no rows\n", path.c_str());
    return false;
  }

  bool ok = true;
  std::size_t mismatches = 0;
  std::set<std::size_t> delays;
  std::map<std::size_t, std::vector<SdfComplex>> tables;

  for (const auto& r : rows) {
    delays.insert(r.d);
    auto it = tables.find(r.d);
    if (it == tables.end()) {
      it = tables.emplace(r.d, twiddleTable(r.d, B_w)).first;
    }
    if (r.k == 0 || r.k >= r.d) {
      std::printf("  %s: row d=%zu k=%zu is out of range\n", path.c_str(), r.d, r.k);
      ok = false;
      continue;
    }
    const auto& w = it->second[r.k];
    if (w.re != r.re || w.im != r.im) {
      ok = false;
      if (++mismatches <= 10) {
        std::printf("  %s: d=%zu k=%zu expected %lld %lld got %d %d\n", path.c_str(), r.d, r.k,
                    static_cast<long long>(r.re), static_cast<long long>(r.im), w.re, w.im);
      }
    }
  }

  // Row count: sum of (d - 1) over the delays the file covers.
  std::size_t expected_rows = 0;
  for (const auto d : delays) {
    expected_rows += d - 1;
  }
  if (rows.size() != expected_rows) {
    std::printf("  %s: %zu rows, expected %zu for delays covered\n", path.c_str(), rows.size(),
                expected_rows);
    ok = false;
  }

  std::printf("  %s: %zu rows, %zu delays, %zu mismatches\n", path.c_str(), rows.size(), delays.size(),
              mismatches);
  return ok;
}

} // namespace

int main() {
  const std::string dir = SMFFT_VECTOR_DIR;

  bool ok = true;
  ok &= checkFile(dir + "/twiddles_N8_W16.txt", 16);
  ok &= checkFile(dir + "/twiddles_N8192_W16.txt", 16);

  // Section 5.1 boundary behaviour, stated explicitly so a change to the
  // clamp or the rounding shows up here and not only as a file diff.
  const auto w90 = twiddle(4, 2, 16); // theta = pi/2 exactly: -sin -> -S
  if (w90.re != 0 || w90.im != -32768) {
    std::printf("  d=4 k=2 expected 0 -32768 got %d %d\n", w90.re, w90.im);
    ok = false;
  }
  const auto w45 = twiddle(4, 1, 16); // cos = sin = 1/sqrt(2)
  if (w45.re != 23170 || w45.im != -23170) {
    std::printf("  d=4 k=1 expected 23170 -23170 got %d %d\n", w45.re, w45.im);
    ok = false;
  }
  // Smallest k of the largest stage: cosine rounds to +S and must clamp down.
  const auto wclamp = twiddle(4096, 1, 16);
  if (wclamp.re != 32767) {
    std::printf("  d=4096 k=1 expected re=32767 (clamped) got %d\n", wclamp.re);
    ok = false;
  }
  // Entry 0 is a deliberate zero, never multiplied.
  const auto table = twiddleTable(8, 16);
  if (table[0].re != 0 || table[0].im != 0) {
    std::printf("  table entry 0 should be {0,0}, got %d %d\n", table[0].re, table[0].im);
    ok = false;
  }

  std::printf("[SDF_TWIDDLE] %s canonical_tables\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
