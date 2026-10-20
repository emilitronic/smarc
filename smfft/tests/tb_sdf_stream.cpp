// **********************************************************************
// smfft/tests/tb_sdf_stream.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
SdfStream against the canonical vectors, under input gaps and downstream
stalls.

Timing is not in doc/fixed-point-spec.md, so there is nothing to compare a
cycle count against. The property that does matter, and the one this checks,
is that flow control changes nothing: the sequence of accepted samples and the
sequence of emitted bins must be exactly the vectors, whatever pattern of
in_val gaps and out_rdy stalls the block is subjected to.

Each case runs three times, and all three must produce identical output:

  always-ready   both sides never block; in_last on the final sample only
  gaps+stalls    deterministic pseudo-random in_val gaps and out_rdy stalls
  flush-each     in_last on every frame boundary, so each frame drains the
                 chain and the next frame starts from a flushed state

The third mode is what tests the chain's counter clear. A flush burns N-1
enabled cycles beyond the frame, so without the clear the counter does not
return to zero and the following frame runs with every stage in the wrong
phase. With in_last only at the end of the stream nothing follows the flush
and the bug is invisible, which is why the first two modes are not enough.

Running all three in one simulation means a difference shows up as one mode
failing and the others passing, rather than as a number to interpret.

Along the way:

  - out_last must fire exactly on the last bin of each frame and nowhere else
  - a bin left waiting by out_rdy must stay valid and unchanged, out_last
    included, until it is taken; the driver likewise holds a sample it
    offered until in_rdy takes it
  - busy must be low before the first sample and after the last bin
  - exactly as many bins must come out as went in

The stall patterns are precomputed and indexed by a cycle counter, not drawn
from in_rdy, so that in_val never depends on in_rdy. Driving valid from ready
is the standard way to produce a combinational cycle through a handshake.
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "SdfStream.hpp"

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

enum Mode { kAlwaysReady = 0, kGapsStalls = 1, kFlushEach = 2 };

const char* kModeNames[] = {"always-ready", "gaps+stalls", "flush-each"};

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
  StreamDriver(const VectorCase& vc, Mode mode, std::string name, COMPONENT_CTOR);

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

  // flush-each costs an extra N-1 cycles per frame on top of the gaps.
  std::size_t budget() const { return 6 * (vc_.in.size() + vc_.params.N) + 64; }
  bool        complete() const { return got_.size() == vc_.expect.size(); }

  bool passed() const {
    return complete() && mismatches_ == 0 && last_errors_ == 0 && busy_errors_ == 0 &&
           hold_errors_ == 0;
  }
  const std::vector<SdfComplex>& got() const { return got_; }
  std::size_t mismatches() const { return mismatches_; }
  std::size_t lastErrors() const { return last_errors_; }
  std::size_t busyErrors() const { return busy_errors_; }
  std::size_t holdErrors() const { return hold_errors_; }

 private:
  const VectorCase& vc_;
  Mode              mode_ = kAlwaysReady;
  std::vector<char> gap_;
  std::vector<char> rdy_;

  std::size_t             cycle_       = 0;
  std::size_t             sent_        = 0;
  std::vector<SdfComplex> got_;
  std::size_t             mismatches_  = 0;
  std::size_t             last_errors_ = 0;
  std::size_t             busy_errors_ = 0;
  std::size_t             hold_errors_ = 0;

  // Handshake hold state: a sample offered and not taken is offered again,
  // and a bin offered and not taken must be offered again unchanged.
  bool       in_held_   = false;
  bool       out_held_  = false;
  SdfComplex held_bits_ = {};
  bit        held_last_ = 0;
};

StreamDriver::StreamDriver(const VectorCase& vc, Mode mode, std::string /*name*/, IMPL_CTOR)
    : vc_(vc), mode_(mode) {
  const std::size_t len = budget();
  // 0% blocked is an always-ready run; only kGapsStalls blocks.
  const unsigned blocked = mode_ == kGapsStalls ? 1u : 0u;
  gap_ = makePattern(len, 0xA5A5F00Dull, blocked ? 35 : 0);
  rdy_ = makePattern(len, 0xC0FFEE11ull, blocked ? 40 : 0);

  UPDATE(updateDrive).writes(in_val, in_bits, in_last, out_rdy);
  UPDATE(updateAccept).reads(in_rdy, out_val, out_bits, out_last, out_rdy, busy);
}

void StreamDriver::updateDrive() {
  const std::size_t c = cycle_ < gap_.size() ? cycle_ : gap_.size() - 1;

  // A gap only starts between samples: once offered, a sample stays on the
  // input until in_rdy takes it, as a ready/valid source must.
  const bool offering = sent_ < vc_.in.size() && (gap_[c] != 0 || in_held_);
  in_val  = bit(offering ? 1 : 0);
  in_bits = offering ? vc_.in[sent_] : SdfComplex{};
  // Flush at every frame boundary in kFlushEach, otherwise once at the end.
  const bool at_frame_end  = ((sent_ + 1) % vc_.params.N) == 0;
  const bool at_stream_end = sent_ + 1 == vc_.in.size();
  const bool last          = mode_ == kFlushEach ? at_frame_end : at_stream_end;
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

  // A bin left waiting by out_rdy must still be there, unchanged, with its
  // out_last.
  if (out_held_ && (out_val != 1 || *out_bits != held_bits_ || out_last != held_last_)) {
    if (hold_errors_ < 4) {
      std::printf("  %s/%s: stalled bin %zu changed at cycle %zu\n", vc_.suite.c_str(),
                  vc_.name.c_str(), got_.size(), cycle_);
    }
    ++hold_errors_;
  }
  out_held_  = out_val == 1 && out_rdy != 1;
  held_bits_ = *out_bits;
  held_last_ = *out_last;
  in_held_   = in_val == 1 && in_rdy != 1;

  if (out_val == 1 && out_rdy == 1) {
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
    ++sent_;
  }
  ++cycle_;
}

void StreamDriver::reset() {
  cycle_       = 0;
  sent_        = 0;
  got_.clear();
  mismatches_  = 0;
  last_errors_ = 0;
  busy_errors_ = 0;
  hold_errors_ = 0;
  in_held_     = false;
  out_held_    = false;
  in_val.reset(0);
  in_bits.reset(SdfComplex{});
  in_last.reset(0);
  out_rdy.reset(0);
}

struct Harness {
  const VectorCase* vc     = nullptr;
  SdfStream*        stream = nullptr;
  StreamDriver*     drv    = nullptr;
  Mode              mode   = kAlwaysReady;
};

Harness build(const VectorCase& vc, Mode mode, Clock& clk) {
  Harness h;
  h.vc     = &vc;
  h.mode   = mode;
  h.stream = new SdfStream("Stream", vc.params);
  h.drv    = new StreamDriver(vc, mode, "Driver");

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

  std::string only;
  std::string only_suite;
  for (int i = 1; i < argc; ++i) {
    if (std::strncmp(argv[i], "-case=", 6) == 0) {
      only = argv[i] + 6;
    } else if (std::strncmp(argv[i], "-suite=", 7) == 0) {
      only_suite = argv[i] + 7;
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
        std::printf("[SDF_STREAM] FAIL could not load %s/%s\n", suite.label, name);
        return 1;
      }
      cases.push_back(vc);
    }
  }
  if (cases.empty()) {
    std::printf("[SDF_STREAM] FAIL no cases selected\n");
    return 1;
  }

  Clock clk;
  std::vector<Harness> hs;
  std::size_t cycles = 0;
  for (auto* vc : cases) {
    for (const Mode mode : {kAlwaysReady, kGapsStalls, kFlushEach}) {
      Harness h = build(*vc, mode, clk);
      cycles    = h.drv->budget() > cycles ? h.drv->budget() : cycles;
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
    const bool pass = h.drv->passed();
    ok &= pass;
    std::printf("  [%s] %-8s %-22s N=%-5zu %-12s %zu/%zu bins", pass ? "PASS" : "FAIL",
                h.vc->suite.c_str(), h.vc->name.c_str(), h.vc->params.N, kModeNames[h.mode],
                h.drv->got().size(), h.vc->expect.size());
    if (h.drv->mismatches() != 0) {
      std::printf("  %zu wrong", h.drv->mismatches());
    }
    if (h.drv->lastErrors() != 0) {
      std::printf("  %zu out_last", h.drv->lastErrors());
    }
    if (h.drv->busyErrors() != 0) {
      std::printf("  %zu busy", h.drv->busyErrors());
    }
    if (h.drv->holdErrors() != 0) {
      std::printf("  %zu stall-hold", h.drv->holdErrors());
    }
    std::printf("\n");
  }

  // The point of running all three: neither flow control nor flushing may
  // change a single value.
  for (std::size_t i = 0; i + 2 < hs.size(); i += 3) {
    for (std::size_t j = 1; j < 3; ++j) {
      if (hs[i].drv->got() != hs[i + j].drv->got()) {
        std::printf("  %s/%s: %s run differs from always-ready run\n",
                    hs[i].vc->suite.c_str(), hs[i].vc->name.c_str(), kModeNames[hs[i + j].mode]);
        ok = false;
      }
    }
  }

  std::printf("[SDF_STREAM] %s %zu runs\n", ok ? "PASS" : "FAIL", hs.size());
  descore::flushLog();
  return ok ? 0 : 1;
}
