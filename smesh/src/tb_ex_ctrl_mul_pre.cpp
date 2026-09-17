// **********************************************************************
// smesh/src/tb_ex_ctrl_mul_pre.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 16 2026
/*
End-to-end ExCtrl COMPUTE + PRELOAD overlap test.

  CONFIG
  PRELOAD0(B0, C0)
  COMPUTE_FLIP0(A0, D0) + PRELOAD1(B1, C1)
  COMPUTE_FLIP1(A1, D1)

  Trace is cumulative progress summary for end-to-end overlap scenario

  - req={b0,b1,b2,b3}: accepted SPAD read requests per physical bank.
      - Bank 0: 8 A rows, four each for A0 and A1.
      - Bank 1: 8 math-D rows, four each for D0 and D1.
      - Bank 2: 4 new-weight rows for B1.
      - Bank 3: 4 initial-weight rows for B0.

  - mesh.req=3: Mesher accepted three operation requests:
      1. Initial PRELOAD0
      2. Overlapped COMPUTE0 + PRELOAD1
      3. Final COMPUTE1

  - mesh.in=12: twelve complete row-beats entered Mesher:
      - 4 final compute rows

    This increments only when A, B, and D complete together. Earlier benign A/B filler handshakes
    are not counted as completed rows.

  - mesh.out=12: twelve rows emerged:
      - 4 non-result preload/drain rows
      - 4 rows of C0 = A0*B0+D0
      - 4 rows of C1 = A1*B1+D1

  - write=8: eight result rows were written to the accumulator:
      - 4 C0 rows
      - 4 C1 rows

  - completion={1,1,1,1,1}: each expected RS tag appeared exactly once, in this array order:

    CONFIG, PRELOAD0, COMPUTE0, PRELOAD1, COMPUTE1
    tags 7, 8, 9, 10, 11


cmake --build build --target tb_ex_ctrl_mul_pre -j >/dev/null 2>&1
./build/smesh/tb_ex_ctrl_mul_pre
./build/smesh/tb_ex_ctrl_mul_pre -trace '*'/mul_pre_view_
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrl.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>

namespace {

constexpr std::uint32_t kB0Base = 12;
constexpr std::uint32_t kA0Base = 0;
constexpr std::uint32_t kD0Base = 4;
constexpr std::uint32_t kB1Base = 8;
constexpr std::uint32_t kA1Base = 0;
constexpr std::uint32_t kD1Base = 4;
constexpr std::uint32_t kC0Base = 0;
constexpr std::uint32_t kC1Base = 8;

constexpr smesh::SmeshRsTag kConfigTag   = 7;
constexpr smesh::SmeshRsTag kPreload0Tag = 8;
constexpr smesh::SmeshRsTag kCompute0Tag = 9;
constexpr smesh::SmeshRsTag kPreload1Tag = 10;
constexpr smesh::SmeshRsTag kCompute1Tag = 11;

smesh::SmeshIssue issue(smesh::SmeshRsTag tag, smesh::SmeshFunct funct,
                        std::uint64_t rs1 = 0, std::uint64_t rs2 = 0) {
  smesh::SmeshIssue out{};
  out.rs_tag_valid = 1;
  out.rs_tag = tag;
  out.cmd.funct = static_cast<std::uint32_t>(funct);
  out.cmd.rs1 = rs1;
  out.cmd.rs2 = rs2;
  return out;
}

const std::array<smesh::SmeshIssue, 5> kProgram{{
    issue(kConfigTag, smesh::SmeshFunct::Config,
          smesh::packConfigExRs1(1), smesh::packConfigExRs2(1)),
    issue(kPreload0Tag, smesh::SmeshFunct::Preload,
          smesh::packLocal(smesh::makeSpAddr(kB0Base), {smesh::kDim, smesh::kDim}),
          smesh::packLocal(smesh::makeAccAddr(kC0Base), {smesh::kDim, smesh::kDim})),
    issue(kCompute0Tag, smesh::SmeshFunct::ComputeFlip,
          smesh::packLocal(smesh::makeSpAddr(kA0Base), {smesh::kDim, smesh::kDim}),
          smesh::packLocal(smesh::makeSpAddr(kD0Base), {smesh::kDim, smesh::kDim})),
    issue(kPreload1Tag, smesh::SmeshFunct::Preload,
          smesh::packLocal(smesh::makeSpAddr(kB1Base), {smesh::kDim, smesh::kDim}),
          smesh::packLocal(smesh::makeAccAddr(kC1Base), {smesh::kDim, smesh::kDim})),
    issue(kCompute1Tag, smesh::SmeshFunct::ComputeFlip,
          smesh::packLocal(smesh::makeSpAddr(kA1Base), {smesh::kDim, smesh::kDim}),
          smesh::packLocal(smesh::makeSpAddr(kD1Base), {smesh::kDim, smesh::kDim})),
}};

smesh::MeshInputRow spadRow(std::uint32_t row) {
  smesh::MeshInputRow data{};
  for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
    data[lane] = static_cast<smesh::Elem>(row * smesh::kDim + lane);
  }
  return data;
}

smesh::MeshAccumRow expectedOutput(std::uint32_t a_row,
                                   std::uint32_t d_row,
                                   std::uint32_t weight_base) {
  smesh::MeshAccumRow out{};
  for (std::size_t col = 0; col < smesh::kDim; ++col) {
    smesh::Acc value = static_cast<smesh::Acc>(d_row * smesh::kDim + col);
    for (std::size_t k = 0; k < smesh::kDim; ++k) {
      const auto a = static_cast<smesh::Acc>(a_row * smesh::kDim + k);
      const auto b = static_cast<smesh::Acc>((weight_base + k) * smesh::kDim + col);
      value += a * b;
    }
    out[col] = value;
  }
  return out;
}

bool equal(const smesh::MeshInputRow& lhs, const smesh::MeshInputRow& rhs) {
  return lhs == rhs;
}

bool equal(const smesh::MeshAccumRow& lhs, const smesh::MeshAccumRow& rhs) {
  return lhs == rhs;
}

} // namespace

TraceKey(mul_pre_view_);

// Keep trace columns aligned while retaining Cascade's normal key filtering.
class FixedWidthCascadeTracer : public descore::Tracer {
 public:
  void traceHeader(const std::string& context, const std::string& keyname) override {
    appendTrace("[%02llu.%03llu] ",
                static_cast<unsigned long long>(Sim::simTime / 1000),
                static_cast<unsigned long long>(Sim::simTime % 1000));
    descore::Tracer::traceHeader(context, keyname);
  }

  bool traceEnabled() const override {
    return Sim::tracing;
  }
};

struct MulPreSpadPipelineState {
  std::array<std::array<bit, smesh::kSpadReadDelay>, smesh::kSpBanks> valid{};
  std::array<std::array<smesh::SpadReadResp, smesh::kSpadReadDelay>, smesh::kSpBanks> bits{};
};

class MulPreSpadModel : public Component {
  DECLARE_COMPONENT(MulPreSpadModel, SpadModel);

 public:
  MulPreSpadModel(std::string name, COMPONENT_CTOR);

  Clock(clk);
  InputArray(bit, req_val, smesh::kSpBanks);
  OutputArray(bit, req_rdy, smesh::kSpBanks);
  InputArray(smesh::SpadBankReadReq, req_bits, smesh::kSpBanks);
  OutputArray(bit, resp_val, smesh::kSpBanks);
  InputArray(bit, resp_rdy, smesh::kSpBanks);
  OutputArray(smesh::SpadReadResp, resp_bits, smesh::kSpBanks);

  void updateRespView();
  void updateReady();
  void updateNextState();
  void reset();

 private:
  Output(bit, active_Q_);
  Register(bit, active_D_);
  Output(MulPreSpadPipelineState, pipeline_Q_);
  Register(MulPreSpadPipelineState, pipeline_D_);
};

MulPreSpadModel::MulPreSpadModel(std::string /*name*/, IMPL_CTOR) {
  static_assert(smesh::kSpadReadDelay > 0);
  active_Q_ <= active_D_;
  pipeline_Q_ <= pipeline_D_;
  UPDATE(updateRespView).reads(pipeline_Q_).writes(resp_val, resp_bits);
  UPDATE(updateReady).reads(active_Q_, resp_rdy, pipeline_Q_).writes(req_rdy);
  UPDATE(updateNextState)
      .reads(active_Q_, req_val, req_rdy, req_bits, resp_rdy, pipeline_Q_)
      .writes(active_D_, pipeline_D_);
}

void MulPreSpadModel::updateRespView() {
  const auto current = *pipeline_Q_;
  const auto last = smesh::kSpadReadDelay - 1;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    resp_val[bank] = current.valid[bank][last];
    resp_bits[bank] = current.bits[bank][last];
  }
}

void MulPreSpadModel::updateReady() {
  const auto current = *pipeline_Q_;
  const auto last = smesh::kSpadReadDelay - 1;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    std::array<bool, smesh::kSpadReadDelay> stage_rdy{};
    stage_rdy[last] = current.valid[bank][last] == 0 || resp_rdy[bank] == 1;
    for (std::size_t stage = last; stage-- > 0;) {
      stage_rdy[stage] = current.valid[bank][stage] == 0 || stage_rdy[stage + 1];
    }
    req_rdy[bank] = bit(active_Q_ == 1 && stage_rdy[0]);
  }
}

void MulPreSpadModel::updateNextState() {
  active_D_ = 1;
  if (Sim::state == Sim::SimResetting || active_Q_ == 0) {
    pipeline_D_ = MulPreSpadPipelineState{};
    return;
  }

  const auto current = *pipeline_Q_;
  auto next = current;
  const auto last = smesh::kSpadReadDelay - 1;

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    std::array<bool, smesh::kSpadReadDelay> stage_rdy{};
    stage_rdy[last] = current.valid[bank][last] == 0 || resp_rdy[bank] == 1;
    for (std::size_t stage = last; stage-- > 0;) {
      stage_rdy[stage] = current.valid[bank][stage] == 0 || stage_rdy[stage + 1];
    }

    for (std::size_t stage = smesh::kSpadReadDelay; stage-- > 1;) {
      if (stage_rdy[stage]) {
        next.valid[bank][stage] = current.valid[bank][stage - 1];
        next.bits[bank][stage] = current.bits[bank][stage - 1];
      }
    }

    if (stage_rdy[0]) {
      const bool fire = req_val[bank] == 1 && req_rdy[bank] == 1;
      next.valid[bank][0] = bit(fire);
      if (fire) {
        const auto local_row = static_cast<std::uint32_t>(req_bits[bank]->addr);
        assert_always(local_row < smesh::kSpBankRows,
                      "SPAD bank %u received out-of-range row %u",
                      static_cast<unsigned>(bank), static_cast<unsigned>(local_row));
        const auto row = static_cast<std::uint32_t>(bank * smesh::kSpBankRows) + local_row;
        smesh::SpadReadResp response{};
        response.data = spadRow(row);
        response.laddr = smesh::makeSpAddr(row);
        response.mask = static_cast<std::uint8_t>((1u << smesh::kDim) - 1u);
        response.len = smesh::kDim;
        response.from_dma = req_bits[bank]->from_dma;
        next.bits[bank][0] = response;
      }
    }
  }

  pipeline_D_ = next;
}

void MulPreSpadModel::reset() {
  active_D_.reset(0);
  pipeline_D_.reset(MulPreSpadPipelineState{});
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    req_rdy[bank].reset(0);
    resp_val[bank].reset(0);
    resp_bits[bank].reset(smesh::SpadReadResp{});
  }
}

class MulPreDriver : public Component {
  DECLARE_COMPONENT(MulPreDriver);

 public:
  MulPreDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoOutput(smesh::SmeshIssue, cmd_out);
  Input(bit, completed_val);
  Input(smesh::SmeshRsTag, completed_bits);
  Input(bit, mesher_req_val);
  Input(bit, mesher_req_rdy);
  Input(smesh::ExCtrlMeshReq, mesher_req_bits);
  Input(bit, mesher_a_val);
  Input(bit, mesher_a_rdy);
  Input(smesh::ExCtrlMeshIn, mesher_a_bits);
  Input(bit, mesher_b_val);
  Input(bit, mesher_b_rdy);
  Input(smesh::ExCtrlMeshIn, mesher_b_bits);
  Input(bit, mesher_d_val);
  Input(bit, mesher_d_rdy);
  Input(smesh::ExCtrlMeshIn, mesher_d_bits);
  Input(bit, mesher_resp_val);
  Input(smesh::MesherResp, mesher_resp_bits);
  InputArray(bit, spad_read_req_val, smesh::kSpBanks);
  InputArray(bit, spad_read_req_rdy, smesh::kSpBanks);
  InputArray(smesh::SpadBankReadReq, spad_read_req_bits, smesh::kSpBanks);
  InputArray(bit, spad_read_resp_val, smesh::kSpBanks);
  InputArray(bit, spad_read_resp_rdy, smesh::kSpBanks);
  InputArray(smesh::SpadReadResp, spad_read_resp_bits, smesh::kSpBanks);
  OutputArray(bit, accum_read_req_rdy, smesh::kAccBanks);
  InputArray(bit, accum_read_req_val, smesh::kAccBanks);
  OutputArray(bit, accum_read_resp_val, smesh::kAccBanks);
  OutputArray(smesh::ExCtrlAccumReadResp, accum_read_resp_bits, smesh::kAccBanks);
  OutputArray(bit, spad_write_rdy, smesh::kSpBanks);
  InputArray(bit, spad_write_val, smesh::kSpBanks);
  OutputArray(bit, accum_write_rdy, smesh::kAccBanks);
  InputArray(bit, accum_write_val, smesh::kAccBanks);
  InputArray(smesh::AccumBankWriteReq, accum_write_bits, smesh::kAccBanks);

  void updateIssue();
  void updateMemoryReady();
  void updateMonitor();
  void reset();

  bool activityComplete() const;
  bool passed() const;
  void report() const;

 private:
  static constexpr std::array<std::size_t, smesh::kSpBanks> kExpectedReads{{8, 8, 4, 4}};

  std::size_t next_issue_ = 0;
  std::array<std::size_t, smesh::kSpBanks> req_count_{};
  std::array<std::size_t, smesh::kSpBanks> resp_count_{};
  std::size_t mesh_req_count_ = 0;
  std::size_t mesh_input_count_ = 0;
  std::size_t mesh_resp_count_ = 0;
  std::size_t write_count_ = 0;
  std::array<std::size_t, 5> completion_count_{};
  bool request_ok_ = true;
  bool response_ok_ = true;
  bool mesh_req_ok_ = true;
  bool mesh_input_ok_ = true;
  bool mesh_resp_ok_ = true;
  bool write_ok_ = true;
  bool completion_ok_ = true;
  bool unexpected_accum_read_ = false;
  bool unexpected_spad_write_ = false;
};

constexpr std::array<std::size_t, smesh::kSpBanks> MulPreDriver::kExpectedReads;

MulPreDriver::MulPreDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateIssue).writes(cmd_out);
  UPDATE(updateMemoryReady)
      .writes(accum_read_req_rdy, accum_read_resp_val, accum_read_resp_bits,
              spad_write_rdy, accum_write_rdy);
  UPDATE(updateMonitor)
      .reads(completed_val, completed_bits,
             mesher_req_val, mesher_req_rdy, mesher_req_bits)
      .reads(mesher_a_val, mesher_a_rdy, mesher_a_bits,
             mesher_b_val, mesher_b_rdy, mesher_b_bits)
      .reads(mesher_d_val, mesher_d_rdy, mesher_d_bits)
      .reads(mesher_resp_val, mesher_resp_bits)
      .reads(spad_read_req_val, spad_read_req_rdy, spad_read_req_bits)
      .reads(spad_read_resp_val, spad_read_resp_rdy, spad_read_resp_bits)
      .reads(accum_read_req_val)
      .reads(spad_write_val)
      .reads(accum_write_val, accum_write_bits);
}

void MulPreDriver::updateIssue() {
  if (Sim::state == Sim::SimResetting || next_issue_ >= kProgram.size() || cmd_out.full()) {
    return;
  }
  cmd_out.push(kProgram[next_issue_++]);
}

void MulPreDriver::updateMemoryReady() {
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    accum_read_req_rdy[bank] = 1;
    accum_read_resp_val[bank] = 0;
    accum_read_resp_bits[bank] = smesh::ExCtrlAccumReadResp{};
    accum_write_rdy[bank] = 1;
  }
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    spad_write_rdy[bank] = 1;
  }
}

void MulPreDriver::updateMonitor() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    if (spad_read_req_val[bank] == 1 && spad_read_req_rdy[bank] == 1) {
      const auto index = req_count_[bank]++;
      const auto row = static_cast<std::uint32_t>(bank * smesh::kSpBankRows) +
                       static_cast<std::uint32_t>(spad_read_req_bits[bank]->addr);
      std::uint32_t expected = 0;
      if (bank == 0) expected = static_cast<std::uint32_t>(index % smesh::kDim);
      if (bank == 1) expected = 4u + static_cast<std::uint32_t>(index % smesh::kDim);
      if (bank == 2) expected = 11u - static_cast<std::uint32_t>(index);
      if (bank == 3) expected = 15u - static_cast<std::uint32_t>(index);
      request_ok_ &= index < kExpectedReads[bank] && row == expected &&
                     spad_read_req_bits[bank]->from_dma == 0;
    }

    if (spad_read_resp_val[bank] == 1 && spad_read_resp_rdy[bank] == 1) {
      const auto index = resp_count_[bank]++;
      const auto& response = *spad_read_resp_bits[bank];
      std::uint32_t expected = 0;
      if (bank == 0) expected = static_cast<std::uint32_t>(index % smesh::kDim);
      if (bank == 1) expected = 4u + static_cast<std::uint32_t>(index % smesh::kDim);
      if (bank == 2) expected = 11u - static_cast<std::uint32_t>(index);
      if (bank == 3) expected = 15u - static_cast<std::uint32_t>(index);
      response_ok_ &= index < kExpectedReads[bank] &&
                      response.laddr.full_sp_addr() == expected &&
                      response.from_dma == 0 && equal(response.data, spadRow(expected));
    }
  }

  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    unexpected_accum_read_ |= accum_read_req_val[bank] == 1;
  }
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    unexpected_spad_write_ |= spad_write_val[bank] == 1;
  }

  if (mesher_req_val == 1 && mesher_req_rdy == 1) {
    const auto request = *mesher_req_bits;
    mesh_req_ok_ &= request.pe_control.dataflow == smesh::kExDataflowWS &&
                    request.total_rows == smesh::kDim && request.flush == 0;
    if (mesh_req_count_ == 0) {
      mesh_req_ok_ &= request.pe_control.propagate == 0 &&
                      request.tag.rs_tag_valid == 1 &&
                      request.tag.rs_tag == kPreload0Tag &&
                      request.tag.addr.raw == smesh::makeAccAddr(kC0Base).raw;
    } else if (mesh_req_count_ == 1) {
      mesh_req_ok_ &= request.pe_control.propagate == 1 &&
                      request.tag.rs_tag_valid == 1 &&
                      request.tag.rs_tag == kPreload1Tag &&
                      request.tag.addr.raw == smesh::makeAccAddr(kC1Base).raw;
    } else if (mesh_req_count_ == 2) {
      mesh_req_ok_ &= request.pe_control.propagate == 1 &&
                      request.tag.rs_tag_valid == 0 && request.tag.addr.is_garbage();
    } else {
      mesh_req_ok_ = false;
    }
    ++mesh_req_count_;
  }

  const bool a_fire = mesher_a_val == 1 && mesher_a_rdy == 1;
  const bool b_fire = mesher_b_val == 1 && mesher_b_rdy == 1;
  const bool d_fire = mesher_d_val == 1 && mesher_d_rdy == 1;
  const bool complete_input_row = a_fire && b_fire && d_fire;
  if ((a_fire || b_fire || d_fire) && !complete_input_row) {
    // Before the first preload response arrives, Mesher may accept the benign
    // A/B fillers without advancing the row. D completes that buffered row.
    mesh_input_ok_ &= mesh_input_count_ == 0 && a_fire && b_fire && !d_fire &&
                      equal(mesher_a_bits->data, smesh::MeshInputRow{}) &&
                      equal(mesher_b_bits->data, smesh::MeshInputRow{});
  }
  if (complete_input_row) {
    const auto row = mesh_input_count_ % smesh::kDim;
    if (mesh_input_count_ < smesh::kDim) {
      mesh_input_ok_ &= a_fire && b_fire && d_fire &&
                        equal(mesher_a_bits->data, smesh::MeshInputRow{}) &&
                        equal(mesher_b_bits->data, smesh::MeshInputRow{}) &&
                        equal(mesher_d_bits->data, spadRow(15u - static_cast<std::uint32_t>(row)));
    } else if (mesh_input_count_ < 2 * smesh::kDim) {
      mesh_input_ok_ &= a_fire && b_fire && d_fire &&
                        equal(mesher_a_bits->data, spadRow(kA0Base + row)) &&
                        equal(mesher_b_bits->data, spadRow(kD0Base + row)) &&
                        equal(mesher_d_bits->data, spadRow(11u - static_cast<std::uint32_t>(row)));
    } else if (mesh_input_count_ < 3 * smesh::kDim) {
      mesh_input_ok_ &= a_fire && b_fire && d_fire &&
                        equal(mesher_a_bits->data, spadRow(kA1Base + row)) &&
                        equal(mesher_b_bits->data, spadRow(kD1Base + row)) &&
                        equal(mesher_d_bits->data, smesh::MeshInputRow{});
    } else {
      mesh_input_ok_ = false;
    }
    ++mesh_input_count_;
  }

  if (mesher_resp_val == 1) {
    const auto response = *mesher_resp_bits;
    const auto row = mesh_resp_count_ % smesh::kDim;
    mesh_resp_ok_ &= (response.last == 1) == (row == smesh::kDim - 1);
    if (mesh_resp_count_ < smesh::kDim) {
      mesh_resp_ok_ &= equal(response.data, smesh::MeshAccumRow{}) &&
                       response.tag.addr.is_garbage();
    } else if (mesh_resp_count_ < 2 * smesh::kDim) {
      mesh_resp_ok_ &= equal(response.data, expectedOutput(kA0Base + row, kD0Base + row, kB0Base)) &&
                       response.tag.rs_tag_valid == 1 && response.tag.rs_tag == kPreload0Tag &&
                       response.tag.addr.raw == smesh::makeAccAddr(kC0Base).raw;
    } else if (mesh_resp_count_ < 3 * smesh::kDim) {
      mesh_resp_ok_ &= equal(response.data, expectedOutput(kA1Base + row, kD1Base + row, kB1Base)) &&
                       response.tag.rs_tag_valid == 1 && response.tag.rs_tag == kPreload1Tag &&
                       response.tag.addr.raw == smesh::makeAccAddr(kC1Base).raw;
    } else {
      mesh_resp_ok_ = false;
    }
    ++mesh_resp_count_;
  }

  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    if (accum_write_val[bank] == 1 && accum_write_rdy[bank] == 1) {
      const auto write = *accum_write_bits[bank];
      const auto result = write_count_ / smesh::kDim;
      const auto row = write_count_ % smesh::kDim;
      if (result < 2) {
        const auto c_base = result == 0 ? kC0Base : kC1Base;
        const auto a_base = result == 0 ? kA0Base : kA1Base;
        const auto d_base = result == 0 ? kD0Base : kD1Base;
        const auto b_base = result == 0 ? kB0Base : kB1Base;
        const auto address = smesh::makeAccAddr(c_base + row);
        write_ok_ &= bank == address.acc_bank() && write.addr == address.acc_row() &&
                     write.mask == 0xffffu && write.acc == 0 &&
                     equal(write.data, expectedOutput(a_base + row, d_base + row, b_base));
      } else {
        write_ok_ = false;
      }
      ++write_count_;
    }
  }

  if (completed_val == 1) {
    const auto tag = static_cast<smesh::SmeshRsTag>(*completed_bits);
    const std::array<smesh::SmeshRsTag, 5> tags{{
        kConfigTag, kPreload0Tag, kCompute0Tag, kPreload1Tag, kCompute1Tag}};
    bool recognized = false;
    for (std::size_t i = 0; i < tags.size(); ++i) {
      if (tag == tags[i]) {
        ++completion_count_[i];
        recognized = true;
      }
    }
    completion_ok_ &= recognized;
    if (tag == kPreload0Tag || tag == kPreload1Tag) {
      completion_ok_ &= mesher_resp_val == 1 && mesher_resp_bits->last == 1 &&
                        mesher_resp_bits->tag.rs_tag_valid == 1 &&
                        mesher_resp_bits->tag.rs_tag == tag;
    }
  }

  s_trace(mul_pre_view_,
          "req={%zu,%zu,%zu,%zu} mesh{req=%zu in=%02zu out=%02zu} write=%zu completion={%zu,%zu,%zu,%zu,%zu}\n",
          req_count_[0], req_count_[1], req_count_[2], req_count_[3],
          mesh_req_count_, mesh_input_count_, mesh_resp_count_, write_count_,
          completion_count_[0], completion_count_[1], completion_count_[2],
          completion_count_[3], completion_count_[4]);
}

void MulPreDriver::reset() {
  next_issue_ = 0;
  req_count_ = {};
  resp_count_ = {};
  mesh_req_count_ = 0;
  mesh_input_count_ = 0;
  mesh_resp_count_ = 0;
  write_count_ = 0;
  completion_count_ = {};
  request_ok_ = true;
  response_ok_ = true;
  mesh_req_ok_ = true;
  mesh_input_ok_ = true;
  mesh_resp_ok_ = true;
  write_ok_ = true;
  completion_ok_ = true;
  unexpected_accum_read_ = false;
  unexpected_spad_write_ = false;
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    accum_read_req_rdy[bank].reset(0);
    accum_read_resp_val[bank].reset(0);
    accum_read_resp_bits[bank].reset(smesh::ExCtrlAccumReadResp{});
    accum_write_rdy[bank].reset(0);
  }
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    spad_write_rdy[bank].reset(0);
  }
}

bool MulPreDriver::activityComplete() const {
  bool counts_ok = next_issue_ == kProgram.size() && mesh_req_count_ == 3 &&
                   mesh_input_count_ == 3 * smesh::kDim &&
                   mesh_resp_count_ == 3 * smesh::kDim && write_count_ == 2 * smesh::kDim;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    counts_ok &= req_count_[bank] == kExpectedReads[bank] &&
                 resp_count_[bank] == kExpectedReads[bank];
  }
  for (const auto count : completion_count_) {
    counts_ok &= count == 1;
  }
  return counts_ok;
}

bool MulPreDriver::passed() const {
  return activityComplete() &&
         request_ok_ && response_ok_ && mesh_req_ok_ && mesh_input_ok_ &&
         mesh_resp_ok_ && write_ok_ && completion_ok_ &&
         !unexpected_accum_read_ && !unexpected_spad_write_;
}

void MulPreDriver::report() const {
  std::printf(
      "  req={%zu,%zu,%zu,%zu} resp={%zu,%zu,%zu,%zu} "
      "mesh={req:%zu in:%zu out:%zu} write=%zu completion={%zu,%zu,%zu,%zu,%zu} "
      "ok={req:%u resp:%u mreq:%u min:%u mout:%u wr:%u comp:%u} unexpected={acc_rd:%u sp_wr:%u}\n",
      req_count_[0], req_count_[1], req_count_[2], req_count_[3],
      resp_count_[0], resp_count_[1], resp_count_[2], resp_count_[3],
      mesh_req_count_, mesh_input_count_, mesh_resp_count_, write_count_,
      completion_count_[0], completion_count_[1], completion_count_[2],
      completion_count_[3], completion_count_[4],
      static_cast<unsigned>(request_ok_), static_cast<unsigned>(response_ok_),
      static_cast<unsigned>(mesh_req_ok_), static_cast<unsigned>(mesh_input_ok_),
      static_cast<unsigned>(mesh_resp_ok_), static_cast<unsigned>(write_ok_),
      static_cast<unsigned>(completion_ok_), static_cast<unsigned>(unexpected_accum_read_),
      static_cast<unsigned>(unexpected_spad_write_));
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::ExCtrl ctrl("ExCtrl");
  MulPreDriver driver("Driver");
  MulPreSpadModel spad("SpadModel");

  ctrl.cmd_in << driver.cmd_out;
  driver.completed_val << ctrl.completed_val;
  driver.completed_bits << ctrl.completed_bits;
  driver.mesher_req_val << ctrl.mesher_req_val;
  driver.mesher_req_rdy << ctrl.mesher_req_rdy;
  driver.mesher_req_bits << ctrl.mesher_req_bits;
  driver.mesher_a_val << ctrl.mesher_a_val;
  driver.mesher_a_rdy << ctrl.mesher_a_rdy;
  driver.mesher_a_bits << ctrl.mesher_a_bits;
  driver.mesher_b_val << ctrl.mesher_b_val;
  driver.mesher_b_rdy << ctrl.mesher_b_rdy;
  driver.mesher_b_bits << ctrl.mesher_b_bits;
  driver.mesher_d_val << ctrl.mesher_d_val;
  driver.mesher_d_rdy << ctrl.mesher_d_rdy;
  driver.mesher_d_bits << ctrl.mesher_d_bits;
  driver.mesher_resp_val << ctrl.mesher_resp_val;
  driver.mesher_resp_bits << ctrl.mesher_resp_bits;

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    spad.req_val[bank] << ctrl.spad_read_req_val[bank];
    spad.req_bits[bank] << ctrl.spad_read_req_bits[bank];
    ctrl.spad_read_req_rdy[bank] << spad.req_rdy[bank];
    ctrl.spad_read_resp_val[bank] << spad.resp_val[bank];
    ctrl.spad_read_resp_bits[bank] << spad.resp_bits[bank];
    spad.resp_rdy[bank] << ctrl.spad_read_resp_rdy[bank];
    driver.spad_read_req_val[bank] << ctrl.spad_read_req_val[bank];
    driver.spad_read_req_rdy[bank] << spad.req_rdy[bank];
    driver.spad_read_req_bits[bank] << ctrl.spad_read_req_bits[bank];
    driver.spad_read_resp_val[bank] << spad.resp_val[bank];
    driver.spad_read_resp_rdy[bank] << ctrl.spad_read_resp_rdy[bank];
    driver.spad_read_resp_bits[bank] << spad.resp_bits[bank];
    ctrl.spad_write_rdy[bank] << driver.spad_write_rdy[bank];
    driver.spad_write_val[bank] << ctrl.spad_write_val[bank];
  }

  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    ctrl.accum_read_req_rdy[bank] << driver.accum_read_req_rdy[bank];
    driver.accum_read_req_val[bank] << ctrl.accum_read_req_val[bank];
    ctrl.accum_read_resp_val[bank] << driver.accum_read_resp_val[bank];
    ctrl.accum_read_resp_bits[bank] << driver.accum_read_resp_bits[bank];
    ctrl.accum_write_rdy[bank] << driver.accum_write_rdy[bank];
    driver.accum_write_val[bank] << ctrl.accum_write_val[bank];
    driver.accum_write_bits[bank] << ctrl.accum_write_bits[bank];
  }

  ctrl.cmd_in.setDelay(1);
  Clock clk;
  ctrl.clk << clk;
  driver.clk << clk;
  spad.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  FixedWidthCascadeTracer fixed_width_tracer;
  auto* previous_tracer = descore::setTracer(&fixed_width_tracer);
  Sim::reset();
  constexpr int kDrainCycles = 4;
  int drain_cycles = 0;
  for (int cycle = 0; cycle < 96; ++cycle) {
    Sim::run();
    if (driver.activityComplete()) {
      ++drain_cycles;
      if (drain_cycles == kDrainCycles) {
        break;
      }
    } else {
      drain_cycles = 0;
    }
  }

  const bool ok = driver.passed();
  std::printf("[EX_CTRL_MUL_PRE] %s compute_preload_overlap\n", ok ? "PASS" : "FAIL");
  if (!ok) {
    driver.report();
  }
  descore::flushLog();
  descore::setTracer(previous_tracer);
  return ok ? 0 : 1;
}
