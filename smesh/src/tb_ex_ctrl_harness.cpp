// **********************************************************************
// smesh/src/tb_ex_ctrl_harness.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 17 2026

#include "tb_ex_ctrl_harness.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <utility>

TraceKey(ex_ctrl_suite_);

namespace smesh {
namespace tb {

namespace {

void buildSpadImage(const ExCtrlTestCase& test,
                    std::array<MeshInputRow, kSpRows>& image,
                    std::array<bool, kSpRows>& initialized) {
  image = {};
  initialized = {};
  for (const auto& matrix : test.spad) {
    assert_always(matrix.location.memory == LocalMemory::Spad,
                  "%s: matrix %s is not in SPAD",
                  test.name.c_str(), matrix.name.c_str());
    assert_always(matrix.rows.size() == matrix.location.shape.rows,
                  "%s: matrix %s has %zu rows but shape says %zu",
                  test.name.c_str(), matrix.name.c_str(), matrix.rows.size(),
                  matrix.location.shape.rows);
    for (std::size_t row = 0; row < matrix.rows.size(); ++row) {
      const auto address = matrix.location.base + static_cast<std::uint32_t>(row);
      assert_always(address < kSpRows,
                    "%s: matrix %s row %u is outside SPAD",
                    test.name.c_str(), matrix.name.c_str(),
                    static_cast<unsigned>(address));
      if (initialized[address]) {
        assert_always(image[address] == matrix.rows[row],
                      "%s: conflicting initializers for SPAD row %u",
                      test.name.c_str(), static_cast<unsigned>(address));
      }
      image[address] = matrix.rows[row];
      initialized[address] = true;
    }
  }
}

MeshAccumRow calculateMatmulRow(
    const ExCtrlTestCase& test,
    const ExpectedMatmul& expected,
    std::size_t row,
    const std::array<MeshInputRow, kSpRows>& image,
    const std::array<bool, kSpRows>& initialized) {
  assert_always(expected.output.memory == LocalMemory::Accum,
                "%s: expected result %s must target accumulator memory",
                test.name.c_str(), expected.name.c_str());
  assert_always(expected.input.memory == LocalMemory::Spad &&
                    expected.weights.memory == LocalMemory::Spad &&
                    expected.addend.memory == LocalMemory::Spad,
                "%s: expected result %s currently requires SPAD operands",
                test.name.c_str(), expected.name.c_str());
  assert_always(expected.input.shape.cols == expected.weights.shape.rows,
                "%s: incompatible A and B shapes for %s",
                test.name.c_str(), expected.name.c_str());

  MeshAccumRow result{};
  const auto a_address = expected.input.base + static_cast<std::uint32_t>(row);
  const auto d_address = expected.addend.base + static_cast<std::uint32_t>(row);
  assert_always(a_address < kSpRows && d_address < kSpRows &&
                    initialized[a_address] && initialized[d_address],
                "%s: uninitialized A or D row for %s",
                test.name.c_str(), expected.name.c_str());

  for (std::size_t col = 0; col < expected.output.shape.cols; ++col) {
    Acc value = static_cast<Acc>(image[d_address][col]);
    for (std::size_t k = 0; k < expected.input.shape.cols; ++k) {
      const auto b_address = expected.weights.base + static_cast<std::uint32_t>(k);
      assert_always(b_address < kSpRows && initialized[b_address],
                    "%s: uninitialized B row for %s",
                    test.name.c_str(), expected.name.c_str());
      value += static_cast<Acc>(image[a_address][k]) *
               static_cast<Acc>(image[b_address][col]);
    }
    result[col] = value;
  }
  return result;
}

std::vector<ExpectedAccumWrite> buildExpectedWrites(
    const ExCtrlTestCase& test,
    const std::array<MeshInputRow, kSpRows>& image,
    const std::array<bool, kSpRows>& initialized) {
  std::vector<ExpectedAccumWrite> writes;
  for (const auto& result : test.expected_results) {
    assert_always(result.output.shape.rows <= kDim &&
                      result.output.shape.cols <= kDim,
                  "%s: expected result %s exceeds mesh dimensions",
                  test.name.c_str(), result.name.c_str());
    for (std::size_t row = 0; row < result.output.shape.rows; ++row) {
      writes.push_back(ExpectedAccumWrite{
          result.name,
          result.output.base + static_cast<std::uint32_t>(row),
          calculateMatmulRow(test, result, row, image, initialized),
      });
    }
  }
  return writes;
}

} // namespace

ExCtrlSuiteSpad::ExCtrlSuiteSpad(const ExCtrlTestCase& test,
                                 std::string /*name*/, IMPL_CTOR)
    : test_(test) {
  static_assert(kSpadReadDelay > 0);
  buildSpadImage(test_, image_, initialized_);
  active_Q_ <= active_D_;
  pipeline_Q_ <= pipeline_D_;
  UPDATE(updateRespView).reads(pipeline_Q_).writes(resp_val, resp_bits);
  UPDATE(updateReady).reads(active_Q_, resp_rdy, pipeline_Q_).writes(req_rdy);
  UPDATE(updateNextState)
      .reads(active_Q_, req_val, req_rdy, req_bits, resp_rdy, pipeline_Q_)
      .writes(active_D_, pipeline_D_);
}

void ExCtrlSuiteSpad::updateRespView() {
  const auto current = *pipeline_Q_;
  const auto last = kSpadReadDelay - 1;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    resp_val[bank] = current.valid[bank][last];
    resp_bits[bank] = current.bits[bank][last];
  }
}

void ExCtrlSuiteSpad::updateReady() {
  const auto current = *pipeline_Q_;
  const auto last = kSpadReadDelay - 1;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    std::array<bool, kSpadReadDelay> stage_rdy{};
    stage_rdy[last] = current.valid[bank][last] == 0 || resp_rdy[bank] == 1;
    for (std::size_t stage = last; stage-- > 0;) {
      stage_rdy[stage] = current.valid[bank][stage] == 0 || stage_rdy[stage + 1];
    }
    req_rdy[bank] = bit(active_Q_ == 1 && stage_rdy[0]);
  }
}

void ExCtrlSuiteSpad::updateNextState() {
  active_D_ = 1;
  if (Sim::state == Sim::SimResetting || active_Q_ == 0) {
    pipeline_D_ = ExCtrlSuiteSpadPipeline{};
    return;
  }

  const auto current = *pipeline_Q_;
  auto next = current;
  const auto last = kSpadReadDelay - 1;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    std::array<bool, kSpadReadDelay> stage_rdy{};
    stage_rdy[last] = current.valid[bank][last] == 0 || resp_rdy[bank] == 1;
    for (std::size_t stage = last; stage-- > 0;) {
      stage_rdy[stage] = current.valid[bank][stage] == 0 || stage_rdy[stage + 1];
    }

    for (std::size_t stage = kSpadReadDelay; stage-- > 1;) {
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
        const auto row = static_cast<std::uint32_t>(bank * kSpBankRows) + local_row;
        assert_always(local_row < kSpBankRows && row < kSpRows,
                      "%s: SPAD bank %u received invalid row %u",
                      test_.name.c_str(), static_cast<unsigned>(bank),
                      static_cast<unsigned>(local_row));
        assert_always(initialized_[row],
                      "%s: read from uninitialized SPAD row %u",
                      test_.name.c_str(), static_cast<unsigned>(row));

        SpadReadResp response{};
        response.data = image_[row];
        response.laddr = makeSpAddr(row);
        response.mask = static_cast<std::uint8_t>((1u << kDim) - 1u);
        response.len = kDim;
        response.from_dma = req_bits[bank]->from_dma;
        next.bits[bank][0] = response;
      }
    }
  }
  pipeline_D_ = next;
}

void ExCtrlSuiteSpad::reset() {
  active_D_.reset(0);
  pipeline_D_.reset(ExCtrlSuiteSpadPipeline{});
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    req_rdy[bank].reset(0);
    resp_val[bank].reset(0);
    resp_bits[bank].reset(SpadReadResp{});
  }
}

ExCtrlSuiteDriver::ExCtrlSuiteDriver(const ExCtrlTestCase& test,
                                     std::string /*name*/, IMPL_CTOR)
    : test_(test), completion_count_(test.expected_completions.size(), 0) {
  buildSpadImage(test_, spad_image_, spad_initialized_);
  expected_writes_ = buildExpectedWrites(test_, spad_image_, spad_initialized_);

  UPDATE(updateIssue).writes(cmd_out);
  UPDATE(updateMemoryReady)
      .writes(accum_read_req_rdy, accum_read_resp_val, accum_read_resp_bits,
              spad_write_rdy, accum_write_rdy);
  UPDATE(updateMonitor)
      .reads(completed_val, completed_bits)
      .reads(mesher_req_val, mesher_req_rdy, mesher_req_bits)
      .reads(mesher_a_val, mesher_a_rdy, mesher_b_val, mesher_b_rdy,
             mesher_d_val, mesher_d_rdy, mesher_resp_val)
      .reads(spad_read_req_val, spad_read_req_rdy, spad_read_req_bits)
      .reads(spad_read_resp_val, spad_read_resp_rdy, spad_read_resp_bits)
      .reads(accum_read_req_val, spad_write_val)
      .reads(accum_write_val, accum_write_rdy, accum_write_bits);
}

void ExCtrlSuiteDriver::updateIssue() {
  if (Sim::state == Sim::SimResetting ||
      next_issue_ >= test_.program.size() || cmd_out.full()) {
    return;
  }
  cmd_out.push(test_.program[next_issue_++]);
}

void ExCtrlSuiteDriver::updateMemoryReady() {
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_read_req_rdy[bank] = 1;
    accum_read_resp_val[bank] = 0;
    accum_read_resp_bits[bank] = ExCtrlAccumReadResp{};
    accum_write_rdy[bank] = 1;
  }
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_write_rdy[bank] = 1;
  }
}

void ExCtrlSuiteDriver::updateMonitor() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (spad_read_req_val[bank] == 1 && spad_read_req_rdy[bank] == 1) {
      const auto index = req_count_[bank]++;
      const auto row = static_cast<std::uint32_t>(bank * kSpBankRows) +
                       static_cast<std::uint32_t>(spad_read_req_bits[bank]->addr);
      request_ok_ &= index < test_.expected_spad_reads[bank].size() &&
                     row == test_.expected_spad_reads[bank][index] &&
                     spad_read_req_bits[bank]->from_dma == 0;
    }

    if (spad_read_resp_val[bank] == 1 && spad_read_resp_rdy[bank] == 1) {
      const auto index = resp_count_[bank]++;
      const auto& response = *spad_read_resp_bits[bank];
      const auto row = static_cast<std::uint32_t>(response.laddr.full_sp_addr());
      response_ok_ &= index < test_.expected_spad_reads[bank].size() &&
                      row == test_.expected_spad_reads[bank][index] &&
                      row < kSpRows && spad_initialized_[row] &&
                      response.data == spad_image_[row] && response.from_dma == 0;
    }
    unexpected_spad_write_ |= spad_write_val[bank] == 1;
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    unexpected_accum_read_ |= accum_read_req_val[bank] == 1;
  }

  if (mesher_req_val == 1 && mesher_req_rdy == 1) {
    const auto& request = *mesher_req_bits;
    mesh_ok_ &= request.pe_control.dataflow == kExDataflowWS &&
                request.total_rows == kDim && request.flush == 0;
    ++mesh_req_count_;
  }

  const bool a_fire = mesher_a_val == 1 && mesher_a_rdy == 1;
  const bool b_fire = mesher_b_val == 1 && mesher_b_rdy == 1;
  const bool d_fire = mesher_d_val == 1 && mesher_d_rdy == 1;
  if (a_fire && b_fire && d_fire) {
    ++mesh_input_count_;
  }
  if (mesher_resp_val == 1) {
    ++mesh_resp_count_;
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (accum_write_val[bank] == 1 && accum_write_rdy[bank] == 1) {
      const auto index = write_count_++;
      if (index >= expected_writes_.size()) {
        write_ok_ = false;
        continue;
      }
      const auto& expected = expected_writes_[index];
      const auto address = makeAccAddr(expected.address);
      const auto& write = *accum_write_bits[bank];
      write_ok_ &= bank == address.acc_bank() &&
                   write.addr == address.acc_row() &&
                   write.data == expected.data && write.acc == 0;
    }
  }

  if (completed_val == 1) {
    const auto tag = static_cast<SmeshRsTag>(*completed_bits);
    const auto found = std::find(test_.expected_completions.begin(),
                                 test_.expected_completions.end(), tag);
    if (found == test_.expected_completions.end()) {
      completion_ok_ = false;
    } else {
      const auto index = static_cast<std::size_t>(
          std::distance(test_.expected_completions.begin(), found));
      ++completion_count_[index];
    }
  }

  std::size_t completion_total = 0;
  for (const auto count : completion_count_) {
    completion_total += count;
  }
  trace(ex_ctrl_suite_,
        "test=%-8s reads={%zu,%zu,%zu,%zu} mesh{req=%zu in=%02zu out=%02zu} "
        "write=%zu completion=%zu/%zu\n",
        test_.name.c_str(),
        req_count_[0], req_count_[1], req_count_[2], req_count_[3],
        mesh_req_count_, mesh_input_count_, mesh_resp_count_, write_count_,
        completion_total, completion_count_.size());
}

void ExCtrlSuiteDriver::reset() {
  next_issue_ = 0;
  req_count_ = {};
  resp_count_ = {};
  mesh_req_count_ = 0;
  mesh_input_count_ = 0;
  mesh_resp_count_ = 0;
  write_count_ = 0;
  completion_count_.assign(test_.expected_completions.size(), 0);
  request_ok_ = true;
  response_ok_ = true;
  mesh_ok_ = true;
  write_ok_ = true;
  completion_ok_ = true;
  unexpected_accum_read_ = false;
  unexpected_spad_write_ = false;
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_read_req_rdy[bank].reset(0);
    accum_read_resp_val[bank].reset(0);
    accum_read_resp_bits[bank].reset(ExCtrlAccumReadResp{});
    accum_write_rdy[bank].reset(0);
  }
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_write_rdy[bank].reset(0);
  }
}

bool ExCtrlSuiteDriver::activityComplete() const {
  bool complete = next_issue_ == test_.program.size() &&
                  mesh_req_count_ == test_.expected_progress.mesh_requests &&
                  mesh_input_count_ == test_.expected_progress.mesh_input_rows &&
                  mesh_resp_count_ == test_.expected_progress.mesh_output_rows &&
                  write_count_ == expected_writes_.size();
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    complete &= req_count_[bank] == test_.expected_spad_reads[bank].size() &&
                resp_count_[bank] == test_.expected_spad_reads[bank].size();
  }
  for (const auto count : completion_count_) {
    complete &= count == 1;
  }
  return complete;
}

bool ExCtrlSuiteDriver::passed() const {
  return activityComplete() && request_ok_ && response_ok_ && mesh_ok_ &&
         write_ok_ && completion_ok_ && !unexpected_accum_read_ &&
         !unexpected_spad_write_;
}

void ExCtrlSuiteDriver::report() const {
  std::size_t completion_total = 0;
  for (const auto count : completion_count_) {
    completion_total += count;
  }
  std::printf(
      "  %s reads={%zu,%zu,%zu,%zu} responses={%zu,%zu,%zu,%zu} "
      "mesh={req:%zu in:%zu out:%zu} writes=%zu/%zu completions=%zu/%zu "
      "ok={req:%u resp:%u mesh:%u write:%u completion:%u} "
      "unexpected={acc_read:%u spad_write:%u}\n",
      test_.name.c_str(),
      req_count_[0], req_count_[1], req_count_[2], req_count_[3],
      resp_count_[0], resp_count_[1], resp_count_[2], resp_count_[3],
      mesh_req_count_, mesh_input_count_, mesh_resp_count_,
      write_count_, expected_writes_.size(), completion_total,
      completion_count_.size(), static_cast<unsigned>(request_ok_),
      static_cast<unsigned>(response_ok_), static_cast<unsigned>(mesh_ok_),
      static_cast<unsigned>(write_ok_), static_cast<unsigned>(completion_ok_),
      static_cast<unsigned>(unexpected_accum_read_),
      static_cast<unsigned>(unexpected_spad_write_));
}

ExCtrlHarnessInstance::ExCtrlHarnessInstance(const ExCtrlTestCase& test,
                                             const std::string& prefix,
                                             Clock& clk) {
  ctrl_.reset(new ExCtrl(prefix + "ExCtrl"));
  driver_.reset(new ExCtrlSuiteDriver(test, prefix + "Driver"));
  spad_.reset(new ExCtrlSuiteSpad(test, prefix + "Spad"));

  ctrl_->cmd_in << driver_->cmd_out;
  driver_->completed_val << ctrl_->completed_val;
  driver_->completed_bits << ctrl_->completed_bits;
  driver_->mesher_req_val << ctrl_->mesher_req_val;
  driver_->mesher_req_rdy << ctrl_->mesher_req_rdy;
  driver_->mesher_req_bits << ctrl_->mesher_req_bits;
  driver_->mesher_a_val << ctrl_->mesher_a_val;
  driver_->mesher_a_rdy << ctrl_->mesher_a_rdy;
  driver_->mesher_b_val << ctrl_->mesher_b_val;
  driver_->mesher_b_rdy << ctrl_->mesher_b_rdy;
  driver_->mesher_d_val << ctrl_->mesher_d_val;
  driver_->mesher_d_rdy << ctrl_->mesher_d_rdy;
  driver_->mesher_resp_val << ctrl_->mesher_resp_val;

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_->req_val[bank] << ctrl_->spad_read_req_val[bank];
    spad_->req_bits[bank] << ctrl_->spad_read_req_bits[bank];
    ctrl_->spad_read_req_rdy[bank] << spad_->req_rdy[bank];
    ctrl_->spad_read_resp_val[bank] << spad_->resp_val[bank];
    ctrl_->spad_read_resp_bits[bank] << spad_->resp_bits[bank];
    spad_->resp_rdy[bank] << ctrl_->spad_read_resp_rdy[bank];

    driver_->spad_read_req_val[bank] << ctrl_->spad_read_req_val[bank];
    driver_->spad_read_req_rdy[bank] << spad_->req_rdy[bank];
    driver_->spad_read_req_bits[bank] << ctrl_->spad_read_req_bits[bank];
    driver_->spad_read_resp_val[bank] << spad_->resp_val[bank];
    driver_->spad_read_resp_rdy[bank] << ctrl_->spad_read_resp_rdy[bank];
    driver_->spad_read_resp_bits[bank] << spad_->resp_bits[bank];

    ctrl_->spad_write_rdy[bank] << driver_->spad_write_rdy[bank];
    driver_->spad_write_val[bank] << ctrl_->spad_write_val[bank];
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    ctrl_->accum_read_req_rdy[bank] << driver_->accum_read_req_rdy[bank];
    driver_->accum_read_req_val[bank] << ctrl_->accum_read_req_val[bank];
    ctrl_->accum_read_resp_val[bank] << driver_->accum_read_resp_val[bank];
    ctrl_->accum_read_resp_bits[bank] << driver_->accum_read_resp_bits[bank];
    ctrl_->accum_write_rdy[bank] << driver_->accum_write_rdy[bank];
    driver_->accum_write_val[bank] << ctrl_->accum_write_val[bank];
    driver_->accum_write_bits[bank] << ctrl_->accum_write_bits[bank];
  }

  ctrl_->cmd_in.setDelay(1);
  ctrl_->clk << clk;
  driver_->clk << clk;
  spad_->clk << clk;
}

bool ExCtrlHarnessInstance::activityComplete() const {
  return driver_->activityComplete();
}

bool ExCtrlHarnessInstance::passed() const {
  return driver_->passed();
}

void ExCtrlHarnessInstance::report() const {
  driver_->report();
}

} // namespace tb
} // namespace smesh
