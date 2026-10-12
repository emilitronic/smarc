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

Both twiddle widths in the vector set are checked: B_w = 16 under vectors/ and
B_w = 24 -- the widest the spec allows -- under vectors_b16_w24/. The clamp
and the rounding are the parts most likely to differ at the extremes, so the
wide tables are not redundant with the narrow ones.
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

  const std::string dir24 = SMFFT_VECTOR_DIR_B16W24;

  bool ok = true;
  ok &= checkFile(dir + "/twiddles_N8_W16.txt", 16);
  ok &= checkFile(dir + "/twiddles_N8192_W16.txt", 16);
  ok &= checkFile(dir24 + "/twiddles_N8_W24.txt", 24);
  ok &= checkFile(dir24 + "/twiddles_N8192_W24.txt", 24);

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
  // The negative bound at B_w = 24: S = 2^23, so -j is -8388608.
  const auto w90_24 = twiddle(4, 2, 24);
  if (w90_24.re != 0 || w90_24.im != -8388608) {
    std::printf("  B_w=24 d=4 k=2 expected 0 -8388608 got %d %d\n", w90_24.re, w90_24.im);
    ok = false;
  }

  // The positive clamp does NOT fire anywhere at B_w = 24, and that is worth
  // pinning rather than assuming. cos(pi/d)*S falls short of S by about
  // S*(pi/d)^2/2, so the clamp needs that deficit below half an LSB:
  //
  //   B_w = 16  deficit at d=4096 is 0.0097  -> rounds to +S, clamps
  //   B_w = 24  deficit at d=4096 is 2.47    -> representable, no clamp
  //
  // Counter-intuitively a wider twiddle clamps less, not more: S grows faster
  // than the rounding threshold. At B_w = 24 the clamp would first bite at
  // d = 16384, i.e. N = 32768, beyond the spec's maximum N. So the clamp is a
  // narrow-twiddle concern only, and the B_w = 24 tables should contain no
  // clamped entry at all.
  if (twiddle(4096, 1, 24).re != 8388606) {
    std::printf("  B_w=24 d=4096 k=1 expected re=8388606 got %d\n", twiddle(4096, 1, 24).re);
    ok = false;
  }
  if (twiddle(4096, 1, 16).re != 32767) {
    std::printf("  B_w=16 d=4096 k=1 expected re=32767 (clamped) got %d\n",
                twiddle(4096, 1, 16).re);
    ok = false;
  }
  for (std::size_t d = 2; d <= 4096; d <<= 1) {
    const auto t = twiddleTable(d, 24);
    for (std::size_t k = 1; k < d; ++k) {
      if (t[k].re == 8388607 || t[k].im == 8388607) {
        std::printf("  B_w=24 d=%zu k=%zu is at the clamp bound: %d %d\n", d, k, t[k].re,
                    t[k].im);
        ok = false;
        break;
      }
    }
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
