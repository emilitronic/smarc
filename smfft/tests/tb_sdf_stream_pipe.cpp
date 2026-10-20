// **********************************************************************
// smfft/tests/tb_sdf_stream_pipe.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
/*
SdfStreamPipe against the canonical vectors, at several pipeline depths,
under input gaps and downstream stalls.

tb_sdf_stream's harness, with the block replaced by the pipelined one and two
additions. Every case runs at each pipeline setting of tb_sdf_chain_pipe --
(0,0) (1,0) (0,1) (1,1) (2,3) and (1,1) with four multipliers -- in four
modes, and all four modes at a setting must produce identical output:

  always-ready   both sides never block; in_last on the final sample only
  gaps+stalls    deterministic pseudo-random in_val gaps and out_rdy stalls
  flush-each     in_last on every frame boundary
  bursts         in_last on every frame boundary, in_val always high, and
                 out_rdy held low for P + 8 cycles at a time

The fourth mode is the one aimed at the new risk. A stall now arrives while P
bins are still inside free-running pipeline registers, and those bins have
to land somewhere. With input running at full rate, a stall longer than P
pushes the whole pipeline's contents into the output queue, so the queue's
high-water mark must reach P -- the bound the reference's P + 1 entries are
sized for. The run fails if it does not, since then the test would not have
exercised what it claims to. The block itself asserts the queue never holds
more than P + 1.

Also checked:

  - latency: in always-ready mode, bin 0 leaves exactly N - 1 + P cycles
    after the first sample is accepted, P being the reference's
    log2(N) * (numAddPipes + complexMulLatency)
  - out_last fires exactly on the last bin of each frame
  - busy is low before the first sample and after the last bin
  - exactly as many bins come out as went in

As in tb_sdf_stream, the stall patterns are precomputed and indexed by cycle
so that in_val never depends on in_rdy.

-case=, -suite= and -pipe=<A>,<M>[,4] narrow the run.
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "SdfStreamPipe.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace smfft;

// A subset of the chain test's cases. Flow control is value-independent, so
// the cases omitted here (n8192_tone_k3001, maxpos, minneg, chirp_echo) add
// simulation time without adding handshake coverage; the four noise-floor
// cases are kept because a rounding difference would show up there first.
const char* kCaseNames[] = {
    "n8_impulse",        "n8_random",             "n16_random",      "n64_tone_k5",
    "n64_random",        "n1024_random",          "n8192_tone_k1",   "n8192_random",
    "n64_quiet_tone_k3", "n8192_quiet_tone_k777", "n8192_lsb_noise", "n8192_weak_echo",
};

struct Suite {
  const char* dir;
  const char* label;
};

enum Mode { kAlwaysReady = 0, kGapsStalls = 1, kFlushEach = 2, kBursts = 3 };
constexpr std::size_t kModes = 4;

const char* kModeNames[] = {"always-ready", "gaps+stalls", "flush-each", "bursts"};

struct PipeSetting {
  SdfPipeParams p;
  std::string   label;
};

PipeSetting mkSetting(std::size_t a, std::size_t m, bool four) {
  PipeSetting s;
  s.p.numAddPipes = a;
  s.p.numMulPipes = m;
  s.p.use4Muls    = four;
  s.label = "(" + std::to_string(a) + "," + std::to_string(m) + ")" + (four ? "4m" : "");
  return s;
}

struct VectorCase {
  std::string             name;
  std::string             suite;
  SdfParams               params;
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

  std::ifstream meta(base + "/meta.txt");
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
    }
  }
  return readSamples(base + "/in.txt", c.in) && readSamples(base + "/out.txt", c.expect) &&
         !c.in.empty() && c.in.size() == c.expect.size();
}

// Deterministic patterns, so a failure reproduces without recording a seed.
std::vector<char> makePattern(std::size_t len, std::uint64_t seed, unsigned percent_blocked) {
  std::vector<char> p(len, 1);
  std::uint64_t     s = seed;
  for (std::size_t i = 0; i < len; ++i) {
    s        = s * 6364136223846793005ull + 1442695040888963407ull;
    const unsigned r = static_cast<unsigned>((s >> 33) % 100);
    p[i]             = r < percent_blocked ? 0 : 1;
  }
  return p;
}

class StreamDriver : public Component {
  DECLARE_COMPONENT(StreamDriver);

 public:
  StreamDriver(const VectorCase& vc, Mode mode, std::size_t P, std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit,        in_val);
  Output(SdfComplex, in_bits);
  Output(bit,        in_last);
  Input(bit,         in_rdy);

  Input(bit,         out_val);
  Input(SdfComplex,  out_bits);
  Input(bit,         out_last);
  Output(bit,        out_rdy);

  Input(bit,         busy);

  void updateDrive();
  void updateAccept();
  void reset();

  // flush-each costs an extra N-1 + P cycles per frame on top of the gaps,
  // and bursts a further P + 8 every few P.
  std::size_t frames() const { return vc_.in.size() / vc_.params.N; }
  std::size_t budget() const {
    return 6 * (vc_.in.size() + vc_.params.N) + (frames() + 2) * 4 * (P_ + 8) + 64;
  }
  bool complete() const { return got_.size() == vc_.expect.size(); }

  // Cycles from the first accepted sample to the first emitted bin.
  bool        latencySeen() const { return first_out_ != kNever && first_in_ != kNever; }
  std::size_t latency() const { return first_out_ - first_in_; }

  bool passed() const {
    return complete() && mismatches_ == 0 && last_errors_ == 0 && busy_errors_ == 0;
  }
  const std::vector<SdfComplex>& got() const { return got_; }
  std::size_t mismatches() const { return mismatches_; }
  std::size_t lastErrors() const { return last_errors_; }
  std::size_t busyErrors() const { return busy_errors_; }

 private:
  static constexpr std::size_t kNever = ~std::size_t{0};

  const VectorCase& vc_;
  Mode              mode_ = kAlwaysReady;
  std::size_t       P_    = 0;
  std::size_t       first_in_  = kNever;
  std::size_t       first_out_ = kNever;
  std::vector<char> gap_;
  std::vector<char> rdy_;

  std::size_t             cycle_       = 0;
  std::size_t             sent_        = 0;
  std::vector<SdfComplex> got_;
  std::size_t             mismatches_  = 0;
  std::size_t             last_errors_ = 0;
  std::size_t             busy_errors_ = 0;
};

StreamDriver::StreamDriver(const VectorCase& vc, Mode mode, std::size_t P, std::string /*name*/,
                           IMPL_CTOR)
    : vc_(vc), mode_(mode), P_(P) {
  const std::size_t len = budget();
  // 0% blocked is an always-ready run; only kGapsStalls blocks at random.
  const unsigned blocked = mode_ == kGapsStalls ? 1u : 0u;
  gap_ = makePattern(len, 0xA5A5F00Dull, blocked ? 35 : 0);
  rdy_ = makePattern(len, 0xC0FFEE11ull, blocked ? 40 : 0);
  if (mode_ == kBursts) {
    // Low for P + 8, high for 3 (P + 8) + 97: long enough to drain, and an
    // odd period so the bursts drift across frame and flush boundaries. The
    // first starts as soon as the first bin can be in the pipeline, so that
    // even a two-frame case is hit while P bins are in flight.
    const std::size_t lo     = P_ + 8;
    const std::size_t period = 4 * lo + 97;
    const std::size_t first  = vc_.params.N - 1 + P_ + 4;
    for (std::size_t i = first; i < len; ++i) {
      rdy_[i] = (i - first) % period < lo ? 0 : 1;
    }
  }

  UPDATE(updateDrive).writes(in_val, in_bits, in_last, out_rdy);
  UPDATE(updateAccept).reads(in_rdy, out_val, out_bits, out_last, out_rdy, busy);
}

void StreamDriver::updateDrive() {
  const std::size_t c = cycle_ < gap_.size() ? cycle_ : gap_.size() - 1;

  const bool offering = sent_ < vc_.in.size() && gap_[c] != 0;
  in_val  = bit(offering ? 1 : 0);
  in_bits = offering ? vc_.in[sent_] : SdfComplex{};
  // Flush at every frame boundary in kFlushEach, otherwise once at the end.
  const bool at_frame_end  = ((sent_ + 1) % vc_.params.N) == 0;
  const bool at_stream_end = sent_ + 1 == vc_.in.size();
  const bool flush_each    = mode_ == kFlushEach || mode_ == kBursts;
  const bool last          = flush_each ? at_frame_end : at_stream_end;
  in_last = bit((offering && last) ? 1 : 0);
  out_rdy = bit(rdy_[c] != 0 ? 1 : 0);
}

void StreamDriver::updateAccept() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  // busy must be low before anything is offered and after everything is out.
  const bool quiet = (sent_ == 0) || complete();
  if (quiet && busy == 1 && cycle_ > 0) {
    if (busy_errors_ < 4) {
      std::printf("  %s/%s: busy high at cycle %zu with sent=%zu got=%zu\n", vc_.suite.c_str(),
                  vc_.name.c_str(), cycle_, sent_, got_.size());
    }
    ++busy_errors_;
  }

  if (out_val == 1 && out_rdy == 1) {
    if (first_out_ == kNever) {
      first_out_ = cycle_;
    }
    const std::size_t m = got_.size();
    if (m < vc_.expect.size()) {
      const auto& expect = vc_.expect[m];
      const auto  actual = *out_bits;
      if (actual != expect) {
        if (mismatches_ < 8) {
          std::printf("  %s/%s: bin %zu expected {%d,%d} got {%d,%d}\n", vc_.suite.c_str(),
                      vc_.name.c_str(), m, expect.re, expect.im, actual.re, actual.im);
        }
        ++mismatches_;
      }
      // out_last marks the last bin of a frame and nothing else.
      const bool want_last = ((m + 1) % vc_.params.N) == 0;
      if ((out_last == 1) != want_last) {
        if (last_errors_ < 4) {
          std::printf("  %s/%s: bin %zu out_last=%u expected %u\n", vc_.suite.c_str(),
                      vc_.name.c_str(), m, static_cast<unsigned>(*out_last), want_last ? 1u : 0u);
        }
        ++last_errors_;
      }
    }
    got_.push_back(*out_bits);
  }

  if (in_val == 1 && in_rdy == 1) {
    if (first_in_ == kNever) {
      first_in_ = cycle_;
    }
    ++sent_;
  }
  ++cycle_;
}

void StreamDriver::reset() {
  cycle_       = 0;
  sent_        = 0;
  first_in_    = kNever;
  first_out_   = kNever;
  got_.clear();
  mismatches_  = 0;
  last_errors_ = 0;
  busy_errors_ = 0;
  in_val.reset(0);
  in_bits.reset(SdfComplex{});
  in_last.reset(0);
  out_rdy.reset(0);
}

struct Harness {
  const VectorCase* vc     = nullptr;
  PipeSetting       setting;
  SdfStreamPipe*    stream = nullptr;
  StreamDriver*     drv    = nullptr;
  Mode              mode   = kAlwaysReady;
};

Harness build(const VectorCase& vc, const PipeSetting& st, Mode mode, Clock& clk) {
  Harness h;
  h.vc      = &vc;
  h.setting = st;
  h.mode    = mode;
  h.stream  = new SdfStreamPipe("StreamPipe", vc.params, st.p);
  h.drv     = new StreamDriver(vc, mode, h.stream->pipeLatency(), "Driver");

  h.stream->in_val  << h.drv->in_val;
  h.stream->in_bits << h.drv->in_bits;
  h.stream->in_last << h.drv->in_last;
  h.drv->in_rdy     << h.stream->in_rdy;

  h.drv->out_val    << h.stream->out_val;
  h.drv->out_bits   << h.stream->out_bits;
  h.drv->out_last   << h.stream->out_last;
  h.stream->out_rdy << h.drv->out_rdy;

  h.drv->busy << h.stream->busy;

  h.stream->clk << clk;
  h.drv->clk    << clk;
  return h;
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
  std::vector<PipeSetting> settings = {mkSetting(0, 0, false), mkSetting(1, 0, false),
                                       mkSetting(0, 1, false), mkSetting(1, 1, false),
                                       mkSetting(2, 3, false), mkSetting(1, 1, true)};
  for (int i = 1; i < argc; ++i) {
    if (std::strncmp(argv[i], "-case=", 6) == 0) {
      only = argv[i] + 6;
    } else if (std::strncmp(argv[i], "-suite=", 7) == 0) {
      only_suite = argv[i] + 7;
    } else if (std::strncmp(argv[i], "-pipe=", 6) == 0) {
      unsigned  a = 0, m = 0, four = 0;
      const int got = std::sscanf(argv[i] + 6, "%u,%u,%u", &a, &m, &four);
      if (got < 2) {
        std::printf("[SDF_STREAM_PIPE] FAIL bad -pipe=%s (want A,M or A,M,4)\n", argv[i] + 6);
        return 1;
      }
      settings = {mkSetting(a, m, got == 3 && four == 4)};
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
        std::printf("[SDF_STREAM_PIPE] FAIL could not load %s/%s\n", suite.label, name);
        return 1;
      }
      cases.push_back(vc);
    }
  }
  if (cases.empty()) {
    std::printf("[SDF_STREAM_PIPE] FAIL no cases selected\n");
    return 1;
  }

  Clock clk;
  std::vector<Harness> hs;
  std::size_t cycles = 0;
  for (const auto& st : settings) {
    for (auto* vc : cases) {
      for (const Mode mode : {kAlwaysReady, kGapsStalls, kFlushEach, kBursts}) {
        Harness h = build(*vc, st, mode, clk);
        cycles    = h.drv->budget() > cycles ? h.drv->budget() : cycles;
        hs.push_back(h);
      }
    }
  }
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();

  // Stop once every run is complete, plus a margin for the busy check, rather
  // than running every harness to the longest budget.
  // Sim::run() is one clock tick; its argument would be picoseconds.
  for (std::size_t c = 0; c < cycles; ++c) {
    Sim::run();
    if ((c & 255) != 255) {
      continue;
    }
    bool all = true;
    for (const auto& h : hs) {
      all = all && h.drv->complete();
    }
    if (all) {
      for (int k = 0; k < 64; ++k) {
        Sim::run();
      }
      break;
    }
  }

  bool ok = true;
  for (const auto& h : hs) {
    const std::size_t P      = h.stream->pipeLatency();
    const std::size_t occ    = h.stream->maxOccupancy();
    const std::size_t expect = h.vc->params.N - 1 + P;

    // Latency is measured where nothing blocks.
    bool latency_ok = true;
    if (h.mode == kAlwaysReady) {
      latency_ok = h.drv->latencySeen() && h.drv->latency() == expect;
    }
    // bursts must actually fill the queue to P, or it tested nothing. Only
    // meaningful where a frame is long enough to keep P bins in flight.
    bool burst_ok = true;
    if (h.mode == kBursts && P > 0 && h.vc->params.N > 2 * P) {
      burst_ok = occ >= P;
    }

    const bool pass = h.drv->passed() && latency_ok && burst_ok;
    ok &= pass;
    std::printf("  [%s] %-6s %-8s %-22s N=%-5zu %-12s P=%-4zu q=%-4zu %zu/%zu bins",
                pass ? "PASS" : "FAIL", h.setting.label.c_str(), h.vc->suite.c_str(),
                h.vc->name.c_str(), h.vc->params.N, kModeNames[h.mode], P, occ,
                h.drv->got().size(), h.vc->expect.size());
    if (h.mode == kAlwaysReady) {
      if (!h.drv->latencySeen()) {
        std::printf("  latency not seen");
      } else {
        std::printf("  lat=%zu%s", h.drv->latency(), latency_ok ? "" : " WRONG");
      }
    }
    if (!burst_ok) {
      std::printf("  queue never reached P");
    }
    if (h.drv->mismatches() != 0) {
      std::printf("  %zu wrong", h.drv->mismatches());
    }
    if (h.drv->lastErrors() != 0) {
      std::printf("  %zu out_last", h.drv->lastErrors());
    }
    if (h.drv->busyErrors() != 0) {
      std::printf("  %zu busy", h.drv->busyErrors());
    }
    std::printf("\n");
  }

  // Neither flow control, flushing, nor pipeline depth may change a value.
  for (std::size_t i = 0; i + kModes - 1 < hs.size(); i += kModes) {
    for (std::size_t j = 1; j < kModes; ++j) {
      if (hs[i].drv->got() != hs[i + j].drv->got()) {
        std::printf("  %s %s/%s: %s run differs from always-ready run\n",
                    hs[i].setting.label.c_str(), hs[i].vc->suite.c_str(),
                    hs[i].vc->name.c_str(), kModeNames[hs[i + j].mode]);
        ok = false;
      }
    }
  }

  std::printf("[SDF_STREAM_PIPE] %s %zu runs\n", ok ? "PASS" : "FAIL", hs.size());
  descore::flushLog();
  return ok ? 0 : 1;
}
