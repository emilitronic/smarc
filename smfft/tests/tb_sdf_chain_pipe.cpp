// **********************************************************************
// smfft/tests/tb_sdf_chain_pipe.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
/*
SdfChainPipe against every case in both vector suites, at several pipeline
depths.

Pipelining moves values in time and must not change one. So the expected
output is the same out.txt tb_sdf_chain uses, and the only thing that moves
is where the harness samples it: output m at cycle m + N - 1 + P, where
P = log2(N) * (A + C) is the reference's pipeline latency
(SDFChainRadix2.scala:247) for that setting.

Settings run, as (numAddPipes, numMulPipes):

  (0,0)          unpipelined; must also match SdfChain cycle for cycle
  (1,0) (0,1)    each knob alone
  (1,1)          both
  (2,3)          deeper, and A != M so a swapped A/M shows up
  (1,1) 4-mult   the other complexMulLatency formula (A + M, not 2A + M)

Three checks per run:

  values     bit-exact against out.txt at the reference latency, and every
             output inside out_width
  latency    for the random cases, sampling one cycle early or one cycle late
             must both fail. Passing at P alone does not prove the latency is
             P -- a constant input passes at any offset -- so this is what
             establishes that the measured latency is exactly the formula.
  identity   at (0,0) the pipelined chain's output equals SdfChain's on every
             cycle, not only on the sampled ones

The input protocol is tb_sdf_chain's: en high throughout, the input followed
by zeros. Stalls are tb_sdf_stream_pipe's job.

-case=, -suite= and -pipe=<A>,<M>[,4] narrow the run.
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "SdfChain.hpp"
#include "SdfChainPipe.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace smfft;

const char* kCaseNames[] = {
    "n8_impulse",        "n8_random",             "n16_random",       "n64_tone_k5",
    "n64_random",        "n1024_random",          "n8192_tone_k1",    "n8192_tone_k3001",
    "n8192_maxpos",      "n8192_minneg",          "n8192_chirp_echo", "n8192_random",
    "n64_quiet_tone_k3", "n8192_quiet_tone_k777", "n8192_lsb_noise",  "n8192_weak_echo",
};

struct Suite {
  const char* dir;
  const char* label;
};

struct PipeSetting {
  SdfPipeParams p;
  std::string   label;
};

std::vector<PipeSetting> defaultSettings() {
  auto mk = [](std::size_t a, std::size_t m, bool four) {
    PipeSetting s;
    s.p.numAddPipes = a;
    s.p.numMulPipes = m;
    s.p.use4Muls    = four;
    s.label = "(" + std::to_string(a) + "," + std::to_string(m) + ")" + (four ? "4m" : "");
    return s;
  };
  return {mk(0, 0, false), mk(1, 0, false), mk(0, 1, false),
          mk(1, 1, false), mk(2, 3, false), mk(1, 1, true)};
}

struct VectorCase {
  std::string             name;
  std::string             suite;
  SdfParams               params;
  std::size_t             out_width = 0;
  std::vector<SdfComplex> in;
  std::vector<SdfComplex> expect;
};

bool readSamples(const std::string& path, std::vector<SdfComplex>& v) {
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
    long long re = 0;
    long long im = 0;
    if (!(s >> re >> im)) {
      return false;
    }
    v.push_back(SdfComplex{static_cast<std::int32_t>(re), static_cast<std::int32_t>(im)});
  }
  return true;
}

bool readCase(const std::string& dir, const std::string& name, VectorCase& c) {
  c.name                 = name;
  const std::string base = dir + "/" + name;
  std::ifstream     meta(base + "/meta.txt");
  if (!meta) {
    std::printf("  cannot open %s/meta.txt\n", base.c_str());
    return false;
  }
  std::string line;
  while (std::getline(meta, line)) {
    const auto eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    const std::string key = line.substr(0, eq);
    const long long   val = std::atoll(line.substr(eq + 1).c_str());
    if (key == "N") {
      c.params.N = static_cast<std::size_t>(val);
    } else if (key == "B_in") {
      c.params.B_in = static_cast<std::size_t>(val);
    } else if (key == "B_w") {
      c.params.B_w = static_cast<std::size_t>(val);
    } else if (key == "out_width") {
      c.out_width = static_cast<std::size_t>(val);
    }
  }
  if (!readSamples(base + "/in.txt", c.in) || !readSamples(base + "/out.txt", c.expect)) {
    return false;
  }
  return !c.in.empty() && c.in.size() == c.expect.size() && c.in.size() % c.params.N == 0 &&
         c.out_width == c.params.outWidth();
}

// Drives one chain (and, at (0,0), the unpipelined reference beside it) and
// records every output cycle; the comparisons are done after the run.
class PipeDriver : public Component {
  DECLARE_COMPONENT(PipeDriver);

 public:
  PipeDriver(const VectorCase& vc, std::size_t pipe_latency, bool with_ref, std::string name,
             COMPONENT_CTOR);

  Clock(clk);
  Output(SdfComplex, in);
  Output(bit,        en);
  Output(bit,        clr);
  Input(SdfComplex,  out);
  Input(SdfComplex,  ref_out); // SdfChain's output; read only when with_ref

  void updateDrive();
  void updateRecord();
  void reset();

  // One spare cycle past the last sampled output, for the late-by-one check.
  std::size_t cycles() const { return vc_.in.size() + vc_.params.N - 1 + P_ + 1; }
  bool        done() const { return step_ >= cycles(); }

  const std::vector<SdfComplex>& seen() const { return seen_; }
  const std::vector<SdfComplex>& refSeen() const { return ref_seen_; }

 private:
  const VectorCase&       vc_;
  std::size_t             P_       = 0;
  bool                    with_ref_ = false;
  std::size_t             step_    = 0;
  std::vector<SdfComplex> seen_;
  std::vector<SdfComplex> ref_seen_;
};

PipeDriver::PipeDriver(const VectorCase& vc, std::size_t pipe_latency, bool with_ref,
                       std::string /*name*/, IMPL_CTOR)
    : vc_(vc), P_(pipe_latency), with_ref_(with_ref) {
  UPDATE(updateDrive).writes(in, en, clr);
  if (with_ref_) {
    UPDATE(updateRecord).reads(out, ref_out);
  } else {
    UPDATE(updateRecord).reads(out);
  }
}

void PipeDriver::updateDrive() {
  clr = 0;
  if (done()) {
    in = SdfComplex{};
    en = 0;
    return;
  }
  in = step_ < vc_.in.size() ? vc_.in[step_] : SdfComplex{};
  en = 1;
}

void PipeDriver::updateRecord() {
  if (Sim::state == Sim::SimResetting || done()) {
    return;
  }
  seen_.push_back(*out);
  if (with_ref_) {
    ref_seen_.push_back(*ref_out);
  }
  ++step_;
}

void PipeDriver::reset() {
  step_ = 0;
  seen_.clear();
  ref_seen_.clear();
  in.reset(SdfComplex{});
  en.reset(0);
  clr.reset(0);
}

struct Harness {
  const VectorCase* vc    = nullptr;
  PipeSetting       setting;
  SdfChainPipe*     chain = nullptr;
  SdfChain*         ref   = nullptr;
  PipeDriver*       drv   = nullptr;
};

// Mismatches when output m is taken at cycle m + lat.
std::size_t countMismatches(const VectorCase& vc, const std::vector<SdfComplex>& seen,
                            std::size_t lat, std::size_t* checked = nullptr) {
  std::size_t bad = 0;
  std::size_t n   = 0;
  for (std::size_t m = 0; m < vc.expect.size(); ++m) {
    const std::size_t c = m + lat;
    if (c >= seen.size()) {
      ++bad;
      continue;
    }
    ++n;
    if (seen[c] != vc.expect[m]) {
      ++bad;
    }
  }
  if (checked) {
    *checked = n;
  }
  return bad;
}

bool parsePipeArg(const char* s, PipeSetting& out) {
  unsigned a = 0, m = 0, four = 0;
  const int got = std::sscanf(s, "%u,%u,%u", &a, &m, &four);
  if (got < 2) {
    return false;
  }
  out.p.numAddPipes = a;
  out.p.numMulPipes = m;
  out.p.use4Muls    = got == 3 && four == 4;
  out.label = "(" + std::to_string(a) + "," + std::to_string(m) + ")" + (out.p.use4Muls ? "4m" : "");
  return true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  const Suite kSuites[] = {
      {SMFFT_VECTOR_DIR, "b13_w16"},
      {SMFFT_VECTOR_DIR_B16W24, "b16_w24"},
  };

  std::string              only;
  std::string              only_suite;
  std::vector<PipeSetting> settings = defaultSettings();
  for (int i = 1; i < argc; ++i) {
    if (std::strncmp(argv[i], "-case=", 6) == 0) {
      only = argv[i] + 6;
    } else if (std::strncmp(argv[i], "-suite=", 7) == 0) {
      only_suite = argv[i] + 7;
    } else if (std::strncmp(argv[i], "-pipe=", 6) == 0) {
      PipeSetting s;
      if (!parsePipeArg(argv[i] + 6, s)) {
        std::printf("[SDF_CHAIN_PIPE] FAIL bad -pipe=%s (want A,M or A,M,4)\n", argv[i] + 6);
        return 1;
      }
      settings = {s};
    }
  }

  std::vector<VectorCase*> cases;
  for (const auto& suite : kSuites) {
    if (!only_suite.empty() && only_suite != suite.label) {
      continue;
    }
    for (const char* name : kCaseNames) {
      if (!only.empty() && only != name) {
        continue;
      }
      auto* vc  = new VectorCase();
      vc->suite = suite.label;
      if (!readCase(suite.dir, name, *vc)) {
        std::printf("[SDF_CHAIN_PIPE] FAIL could not load %s/%s\n", suite.label, name);
        return 1;
      }
      cases.push_back(vc);
    }
  }
  if (cases.empty()) {
    std::printf("[SDF_CHAIN_PIPE] FAIL no cases selected\n");
    return 1;
  }

  Clock                clk;
  std::vector<Harness> hs;
  std::size_t          cycles = 0;
  for (const auto& st : settings) {
    for (auto* vc : cases) {
      Harness h;
      h.vc      = vc;
      h.setting = st;
      h.chain   = new SdfChainPipe("ChainPipe", vc->params, st.p);
      const bool unpiped = st.p.stageLatency() == 0;
      h.drv     = new PipeDriver(*vc, h.chain->pipeLatency(), unpiped, "Driver");

      h.chain->in  << h.drv->in;
      h.chain->en  << h.drv->en;
      h.chain->clr << h.drv->clr;
      h.drv->out   << h.chain->out;
      h.chain->clk << clk;
      h.drv->clk   << clk;

      if (unpiped) {
        h.ref = new SdfChain("Chain", vc->params);
        h.ref->in  << h.drv->in;
        h.ref->en  << h.drv->en;
        h.ref->clr << h.drv->clr;
        h.drv->ref_out << h.ref->out;
        h.ref->clk << clk;
      } else {
        h.drv->ref_out << h.chain->out; // unread; every input needs a driver
      }

      cycles = h.drv->cycles() > cycles ? h.drv->cycles() : cycles;
      hs.push_back(h);
    }
  }
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (std::size_t c = 0; c < cycles; ++c) {
    Sim::run();
  }

  bool ok = true;
  for (const auto& h : hs) {
    const VectorCase& vc  = *h.vc;
    const auto&       sn  = h.drv->seen();
    const std::size_t lat = h.chain->latency();

    std::size_t       checked = 0;
    const std::size_t bad     = countMismatches(vc, sn, lat, &checked);

    std::size_t overflows = 0;
    const std::int32_t lo = -(std::int32_t{1} << (vc.out_width - 1));
    const std::int32_t hi = (std::int32_t{1} << (vc.out_width - 1)) - 1;
    for (std::size_t m = 0; m < vc.expect.size() && m + lat < sn.size(); ++m) {
      const auto& v = sn[m + lat];
      if (v.re < lo || v.re > hi || v.im < lo || v.im > hi) {
        ++overflows;
      }
    }

    // Only the random inputs are rich enough for a shifted sampling point to
    // be certain to fail.
    const bool latency_probe = vc.name.find("random") != std::string::npos;
    bool       latency_ok    = true;
    if (latency_probe) {
      const std::size_t early = countMismatches(vc, sn, lat - 1);
      const std::size_t late  = countMismatches(vc, sn, lat + 1);
      latency_ok              = early > 0 && late > 0;
    }

    bool        identity_ok = true;
    std::size_t identity_diffs = 0;
    if (h.ref != nullptr) {
      const auto& rs = h.drv->refSeen();
      identity_ok    = rs.size() == sn.size();
      for (std::size_t c = 0; c < rs.size() && c < sn.size(); ++c) {
        if (rs[c] != sn[c]) {
          ++identity_diffs;
        }
      }
      identity_ok = identity_ok && identity_diffs == 0;
    }

    const bool pass = bad == 0 && overflows == 0 && checked == vc.expect.size() && latency_ok &&
                      identity_ok;
    ok &= pass;
    std::printf("  [%s] %-6s %-8s %-22s N=%-5zu P=%-4zu %zu/%zu outputs", pass ? "PASS" : "FAIL",
                h.setting.label.c_str(), vc.suite.c_str(), vc.name.c_str(), vc.params.N,
                h.chain->pipeLatency(), checked - (bad > checked ? checked : bad),
                vc.expect.size());
    if (bad != 0) {
      std::printf("  %zu mismatches", bad);
    }
    if (overflows != 0) {
      std::printf("  %zu overflows", overflows);
    }
    if (latency_probe) {
      std::printf("  latency %s", latency_ok ? "exact" : "NOT UNIQUE");
    }
    if (h.ref != nullptr) {
      std::printf("  %s", identity_ok ? "== SdfChain" : "differs from SdfChain");
      if (identity_diffs != 0) {
        std::printf(" on %zu cycles", identity_diffs);
      }
    }
    std::printf("\n");
  }

  std::printf("[SDF_CHAIN_PIPE] %s %zu runs\n", ok ? "PASS" : "FAIL", hs.size());
  descore::flushLog();
  return ok ? 0 : 1;
}
