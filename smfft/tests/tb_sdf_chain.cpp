// **********************************************************************
// smfft/tests/tb_sdf_chain.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
The acceptance test: SdfChain against every case in tests/vectors/.

This is the first thing in smfft that checks the contract rather than a piece
of it. doc/fixed-point-spec.md requires the model to reproduce each case's
out.txt line for line, bit-exact, and that is what is compared here -- all
twelve cases, from n8_impulse up to the three 8192-point ones.

SdfChain has no flow control, so the protocol is the harness's job and it is
the one in SdfStream.run(): feed the whole input with en high, keep stepping
for a further N-1 cycles with zeros to flush, and discard the first N-1
outputs. Output m of the file is therefore the chain's output at cycle
m + N - 1. Multi-frame cases need nothing extra; frames stream back to back.

Every case is instantiated into one simulation and they run concurrently, each
driver idling once its own script is spent. A single case can be selected with
-case=<name> when debugging, which is also the only way to make -trace output
readable.

Also checked, per spec section 3: every output fits the declared out_width,
since v0.1 grows the word by one bit per stage specifically so that nothing
overflows and no saturation is needed. A value that does not fit means the
growth rule is wrong, not that the vector is.

Output order is bit-reversed (spec section 6) -- out.txt is already in that
order, so nothing here unscrambles anything.
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "SdfChain.hpp"

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
    "n8_impulse", "n8_random",      "n16_random",    "n64_tone_k5",
    "n64_random", "n1024_random",   "n8192_tone_k1", "n8192_tone_k3001",
    "n8192_maxpos", "n8192_minneg", "n8192_chirp_echo", "n8192_random",
};

struct VectorCase {
  std::string             name;
  SdfParams               params;
  std::size_t             out_width = 0;
  std::string             description;
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
      std::printf("  malformed sample in %s: %s\n", path.c_str(), line.c_str());
      return false;
    }
    v.push_back(SdfComplex{static_cast<std::int32_t>(re), static_cast<std::int32_t>(im)});
  }
  return true;
}

bool readCase(const std::string& dir, const std::string& name, VectorCase& c) {
  c.name = name;
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
    const std::string val = line.substr(eq + 1);
    if (key == "N") {
      c.params.N = static_cast<std::size_t>(std::atoll(val.c_str()));
    } else if (key == "B_in") {
      c.params.B_in = static_cast<std::size_t>(std::atoll(val.c_str()));
    } else if (key == "B_w") {
      c.params.B_w = static_cast<std::size_t>(std::atoll(val.c_str()));
    } else if (key == "out_width") {
      c.out_width = static_cast<std::size_t>(std::atoll(val.c_str()));
    } else if (key == "description") {
      c.description = val;
    }
  }

  if (!readSamples(base + "/in.txt", c.in) || !readSamples(base + "/out.txt", c.expect)) {
    return false;
  }
  if (c.in.empty() || c.in.size() != c.expect.size()) {
    std::printf("  %s: %zu inputs but %zu outputs\n", name.c_str(), c.in.size(), c.expect.size());
    return false;
  }
  if (c.in.size() % c.params.N != 0) {
    std::printf("  %s: %zu samples is not a whole number of N=%zu frames\n", name.c_str(),
                c.in.size(), c.params.N);
    return false;
  }
  // The declared width must be the one the growth rule gives, or the case is
  // testing something other than what the spec says.
  if (c.out_width != c.params.outWidth()) {
    std::printf("  %s: meta says out_width=%zu, spec growth gives %zu\n", name.c_str(),
                c.out_width, c.params.outWidth());
    return false;
  }
  return true;
}

class ChainDriver : public Component {
  DECLARE_COMPONENT(ChainDriver);

 public:
  ChainDriver(const VectorCase& vc, std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(SdfComplex, in);
  Output(bit,        en);
  Input(SdfComplex,  out);

  void updateDrive();
  void updateCheck();
  void reset();

  std::size_t cycles() const { return vc_.in.size() + vc_.params.N - 1; }
  bool        done() const { return step_ >= cycles(); }
  bool        passed() const { return mismatches_ == 0 && overflows_ == 0; }
  std::size_t checked() const { return checked_; }
  std::size_t mismatches() const { return mismatches_; }
  std::size_t overflows() const { return overflows_; }

 private:
  const VectorCase& vc_;
  std::size_t       step_        = 0;
  std::size_t       checked_     = 0;
  std::size_t       mismatches_  = 0;
  std::size_t       overflows_   = 0;
};

ChainDriver::ChainDriver(const VectorCase& vc, std::string /*name*/, IMPL_CTOR) : vc_(vc) {
  UPDATE(updateDrive).writes(in, en);
  UPDATE(updateCheck).reads(out);
}

void ChainDriver::updateDrive() {
  if (done()) {
    in = SdfComplex{};
    en = 0;
    return;
  }
  // Past the end of the input the chain is flushed with real zero samples.
  // Dropping en instead would only stall it; the transform still needs N
  // samples through every stage.
  in = step_ < vc_.in.size() ? vc_.in[step_] : SdfComplex{};
  en = 1;
}

void ChainDriver::updateCheck() {
  if (Sim::state == Sim::SimResetting || done()) {
    return;
  }
  const std::size_t lat = vc_.params.N - 1;
  if (step_ >= lat) {
    const std::size_t m = step_ - lat;
    if (m < vc_.expect.size()) {
      const auto& expect = vc_.expect[m];
      const auto  actual = *out;
      if (actual != expect) {
        if (mismatches_ < 8) {
          std::printf("  %s: output %zu expected {%d,%d} got {%d,%d}\n", vc_.name.c_str(), m,
                      expect.re, expect.im, actual.re, actual.im);
        }
        ++mismatches_;
      }
      const std::int32_t lo = -(std::int32_t{1} << (vc_.out_width - 1));
      const std::int32_t hi = (std::int32_t{1} << (vc_.out_width - 1)) - 1;
      if (actual.re < lo || actual.re > hi || actual.im < lo || actual.im > hi) {
        if (overflows_ < 4) {
          std::printf("  %s: output %zu {%d,%d} does not fit %zu bits\n", vc_.name.c_str(), m,
                      actual.re, actual.im, vc_.out_width);
        }
        ++overflows_;
      }
      ++checked_;
    }
  }
  ++step_;
}

void ChainDriver::reset() {
  step_       = 0;
  checked_    = 0;
  mismatches_ = 0;
  overflows_  = 0;
  in.reset(SdfComplex{});
  en.reset(0);
}

struct Harness {
  const VectorCase* vc    = nullptr;
  SdfChain*         chain = nullptr;
  ChainDriver*      drv   = nullptr;
};

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  const std::string dir = SMFFT_VECTOR_DIR;

  // -case=<name> narrows the run to one case, for debugging and tracing.
  std::string only;
  for (int i = 1; i < argc; ++i) {
    if (std::strncmp(argv[i], "-case=", 6) == 0) {
      only = argv[i] + 6;
    }
  }

  std::vector<VectorCase*> cases;
  for (const char* name : kCaseNames) {
    if (!only.empty() && only != name) {
      continue;
    }
    auto* vc = new VectorCase();
    if (!readCase(dir, name, *vc)) {
      std::printf("[SDF_CHAIN] FAIL could not load %s\n", name);
      return 1;
    }
    cases.push_back(vc);
  }
  if (cases.empty()) {
    std::printf("[SDF_CHAIN] FAIL no cases selected\n");
    return 1;
  }

  Clock clk;
  std::vector<Harness> hs;
  std::size_t cycles = 0;
  for (auto* vc : cases) {
    Harness h;
    h.vc    = vc;
    h.chain = new SdfChain("Chain", vc->params);
    h.drv   = new ChainDriver(*vc, "Driver");

    h.chain->in << h.drv->in;
    h.chain->en << h.drv->en;
    h.drv->out  << h.chain->out;

    h.chain->clk << clk;
    h.drv->clk   << clk;

    cycles = h.drv->cycles() > cycles ? h.drv->cycles() : cycles;
    hs.push_back(h);
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
    const bool pass = h.drv->passed() && h.drv->done() && h.drv->checked() == h.vc->expect.size();
    ok &= pass;
    std::printf("  [%s] %-18s N=%-5zu %zu/%zu outputs", pass ? "PASS" : "FAIL", h.vc->name.c_str(),
                h.vc->params.N, h.drv->checked(), h.vc->expect.size());
    if (h.drv->mismatches() != 0) {
      std::printf("  %zu mismatches", h.drv->mismatches());
    }
    if (h.drv->overflows() != 0) {
      std::printf("  %zu overflows", h.drv->overflows());
    }
    std::printf("\n");
  }

  std::printf("[SDF_CHAIN] %s %zu cases\n", ok ? "PASS" : "FAIL", hs.size());
  descore::flushLog();
  return ok ? 0 : 1;
}
