// **********************************************************************
// smesh/tests/integration/ex_ctrl/tb_ex_ctrl_harness.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 17 2026

#include "tb_ex_ctrl_harness.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <utility>

// Harness-owned trace views; production-component trace keys remain available too.
// Harness traces: observe interfaces around ExCtrl; emitted by test driver or memory model.
TraceKey(ex_ctrl_suite_);         // Cumulative test progress: read counts, Mesher countes, writes, completions (printed every monitored cyc)
TraceKey(ex_ctrl_view);           // ExCtrlSuiteDriver: test-only taps from ExCtrl: FSM state and cmd queue (printed every monitored cyc)
TraceKey(ex_ctrl_spad_mem_view);  // ExCtrlSuiteSpad: req/resp activity insite test SPAD model (printed when req/resp activity occurs)
TraceKey(ex_ctrl_write_mem_view); // ExCtrlSuiteDriver: ExCtrl's observed spad/accum write ports (printed when write activity occurs)
TraceKey(ex_ctrl_completed_view); // ExCtrlSuiteDriver: ExCtrl's observed completion output (printed when completed_val asserted)
// Production traces: observe behavoiur inside ExCtrl's components; emitted directly by Meshter, MQ, RowFeet, etc.
// Enter these at the command-line, e.g.,
// ./build/smesh/tb_ex_ctrl_suite -test=basic -trace '*/ex_ctrl_view;*/ex_ctrl_spad_mem_view;*/ex_ctrl_write_mem_view;*/ex_ctrl_completed_view;*/mesher_resp_'

namespace smesh {
namespace tb {

namespace {

// Convert encoded commands and FSM states into compact trace labels.
const char* functName(std::uint32_t funct) {
  switch (static_cast<SmeshFunct>(funct)) {
    case SmeshFunct::Config:      return "CFG";
    case SmeshFunct::Mvin2:       return "M2";
    case SmeshFunct::Mvin:        return "MVI";
    case SmeshFunct::Mvout:       return "MVO";
    case SmeshFunct::ComputeFlip: return "CMPF";
    case SmeshFunct::ComputeStay: return "CMPS";
    case SmeshFunct::Preload:     return "PRE";
    case SmeshFunct::Flush:       return "FLU";
    case SmeshFunct::Mvin3:       return "M3";
    case SmeshFunct::StoreSpad:   return "SSP";
  }
  return "---";
}

const char* commandName(bool valid, std::uint32_t funct) {
  return valid ? functName(funct) : "---";
}

const char* stateName(std::uint8_t state) {
  switch (static_cast<ExCtrlFsmState>(state)) {
    case ExCtrlFsmState::WaitingForCmd: return "WAIT";
    case ExCtrlFsmState::Compute:       return "COMP";
    case ExCtrlFsmState::Flush:         return "FLSH";
    case ExCtrlFsmState::Flushing:      return "FLSG";
  }
  return "?";
}

// Compare a request with the control and destination metadata declared by the scenario.
bool matchesMesherRequest(const ExCtrlMeshReq& actual,
                          const ExpectedMesherRequest& expected) {
  const bool destination_matches = expected.destination_garbage
      ? actual.tag.addr.is_garbage()
      : actual.tag.addr.raw == expected.destination.raw &&
            actual.tag.rows == expected.rows && actual.tag.cols == expected.cols;
  return actual.pe_control.dataflow == kExDataflowWS &&
         actual.pe_control.propagate == expected.propagate &&
         actual.pe_control.shift == 0 && actual.a_transpose == 0 &&
         actual.bd_transpose == 0 && actual.total_rows == kDim &&
         actual.tag.rs_tag_valid == expected.rs_tag_valid &&
         (!expected.rs_tag_valid || actual.tag.rs_tag == expected.rs_tag) &&
         destination_matches && actual.flush == 0;
}

// Compare a mesh output row, including the metadata that will drive writeback.
bool matchesMesherResponse(const MesherResp& actual,
                           const ExpectedMesherResponse& expected) {
  const bool destination_matches = expected.destination_garbage
      ? actual.tag.addr.is_garbage()
      : actual.tag.addr.raw == expected.destination.raw &&
            actual.tag.rows == expected.rows && actual.tag.cols == expected.cols;
  return actual.data == expected.data && actual.total_rows == kDim &&
         actual.tag.rs_tag_valid == expected.rs_tag_valid &&
         (!expected.rs_tag_valid || actual.tag.rs_tag == expected.rs_tag) &&
         destination_matches && actual.last == expected.last;
}

// Expand the scenario's named matrices into the scratchpad image used by the model.
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

// Calculate one golden accumulator row for C=A*B+D directly from test data.
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

// Convert all expected matrices into the ordered accumulator writes ExCtrl should emit.
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

// ************************
// ********* SPAD *********
// Elastic fixed-latency SPAD pipeline model used only by this testbench.
// Build a banked, fixed-latency scratchpad model with explicitly registered pipeline state.
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

// Present the last pipeline stage as the current scratchpad response.
void ExCtrlSuiteSpad::updateRespView() {
  const auto current = *pipeline_Q_;
  const auto last = kSpadReadDelay - 1;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    resp_val[bank] = current.valid[bank][last];
    resp_bits[bank] = current.bits[bank][last];
    if (current.valid[bank][last] == 1) {
      const auto& response = current.bits[bank][last];
      trace(ex_ctrl_spad_mem_view,
            "offer bank=%u row=%u data={%d,%d,%d,%d}\n",
            static_cast<unsigned>(bank),
            static_cast<unsigned>(response.laddr.full_sp_addr()),
            static_cast<int>(response.data[0]),
            static_cast<int>(response.data[1]),
            static_cast<int>(response.data[2]),
            static_cast<int>(response.data[3]));
    }
  }
}

// Backpressure propagates from the response port toward the request port.
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

// Advance accepted requests through the elastic response pipeline for the next cycle.
void ExCtrlSuiteSpad::updateNextState() {
  active_D_ = 1;
  // Reset traffic is not real traffic, so begin with an empty response pipeline.
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

    // Move each occupied stage only when the following stage can accept it.
    for (std::size_t stage = kSpadReadDelay; stage-- > 1;) {
      if (stage_rdy[stage]) {
        next.valid[bank][stage] = current.valid[bank][stage - 1];
        next.bits[bank][stage] = current.bits[bank][stage - 1];
      }
    }

    // Turn each accepted bank-local request into a full-address response row.
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
        trace(ex_ctrl_spad_mem_view,
              "req  bank=%u row=%u data={%d,%d,%d,%d}\n",
              static_cast<unsigned>(bank),
              static_cast<unsigned>(response.laddr.full_sp_addr()),
              static_cast<int>(response.data[0]),
              static_cast<int>(response.data[1]),
              static_cast<int>(response.data[2]),
              static_cast<int>(response.data[3]));
      }
    }

    if (current.valid[bank][last] == 1 && resp_rdy[bank] == 1) {
      const auto& response = current.bits[bank][last];
      trace(ex_ctrl_spad_mem_view,
            "take  bank=%u row=%u data={%d,%d,%d,%d}\n",
            static_cast<unsigned>(bank),
            static_cast<unsigned>(response.laddr.full_sp_addr()),
            static_cast<int>(response.data[0]),
            static_cast<int>(response.data[1]),
            static_cast<int>(response.data[2]),
            static_cast<int>(response.data[3]));
    }
  }
  pipeline_D_ = next;
}

// Initialize all registered state and externally visible memory ports.
void ExCtrlSuiteSpad::reset() {
  active_D_.reset(0);
  pipeline_D_.reset(ExCtrlSuiteSpadPipeline{});
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    req_rdy[bank].reset(0);
    resp_val[bank].reset(0);
    resp_bits[bank].reset(SpadReadResp{});
  }
}
// ********* SPAD *********
// ************************

// Build the command source, passive monitor, and expected-result checker.
ExCtrlSuiteDriver::ExCtrlSuiteDriver(const ExCtrlTestCase& test,
                                     std::string /*name*/, IMPL_CTOR)
    : test_(test), completion_count_(test.expected_completions.size(), 0) {
  buildSpadImage(test_, spad_image_, spad_initialized_);
  expected_writes_ = buildExpectedWrites(test_, spad_image_, spad_initialized_);
  assert_always(test_.expected_mesh_requests.size() ==
                    test_.expected_progress.mesh_requests,
                "%s: mesh request expectations disagree with progress count",
                test_.name.c_str());
  assert_always(test_.expected_mesh_inputs.size() ==
                    test_.expected_progress.mesh_input_rows,
                "%s: mesh input expectations disagree with progress count",
                test_.name.c_str());
  assert_always(test_.expected_mesh_responses.size() ==
                    test_.expected_progress.mesh_output_rows,
                "%s: mesh response expectations disagree with progress count",
                test_.name.c_str());

  UPDATE(updateIssue).reads(cmd_rdy).writes(cmd_val, cmd_bits);
  UPDATE(updateMemoryReady)
      .writes(accum_read_req_rdy, accum_read_resp_val, accum_read_resp_bits,
              spad_write_rdy, accum_write_rdy);
  UPDATE(updateMonitor)
      .reads(completed_val, completed_bits)
      .reads(control_state, cmd_queue_head_val, cmd_queue_head_bits)
      .reads(mesher_req_val, mesher_req_rdy, mesher_req_bits)
      .reads(mesher_a_val, mesher_a_rdy, mesher_a_bits,
             mesher_b_val, mesher_b_rdy, mesher_b_bits,
             mesher_d_val, mesher_d_rdy)
      .reads(mesher_d_bits, mesher_resp_val, mesher_resp_bits)
      .reads(spad_read_req_val, spad_read_req_rdy, spad_read_req_bits)
      .reads(spad_read_resp_val, spad_read_resp_rdy, spad_read_resp_bits)
      .reads(accum_read_req_val, spad_write_val, spad_write_bits)
      .reads(accum_write_val, accum_write_rdy, accum_write_bits);
}

// Feed the scenario's commands into ExCtrl in program order when its queue accepts them.
void ExCtrlSuiteDriver::updateIssue() {
  cmd_val = 0;
  cmd_bits = SmeshIssue{};
  if (Sim::state == Sim::SimResetting ||
      next_issue_ >= test_.program.size()) {
    return;
  }
  cmd_val = 1;
  cmd_bits = test_.program[next_issue_];
  if (cmd_rdy == 1) {
    ++next_issue_;
  }
}

// Model always-ready write ports and an unused accumulator-read response path.
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

// Observe each external ExCtrl transaction and compare it with the scenario expectations.
void ExCtrlSuiteDriver::updateMonitor() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  // Show the FSM state and the three command-queue entries visible to ExCtrl.
  trace(ex_ctrl_view,
        "state=%4s h0{v=%u t=%03u c=%4s} h1{v=%u t=%03u c=%4s} "
        "h2{v=%u t=%03u c=%4s}\n",
        stateName(static_cast<std::uint8_t>(*control_state)),
        static_cast<unsigned>(cmd_queue_head_val[0]),
        static_cast<unsigned>(cmd_queue_head_bits[0]->rs_tag),
        commandName(cmd_queue_head_val[0] == 1,
                    cmd_queue_head_bits[0]->cmd.funct),
        static_cast<unsigned>(cmd_queue_head_val[1]),
        static_cast<unsigned>(cmd_queue_head_bits[1]->rs_tag),
        commandName(cmd_queue_head_val[1] == 1,
                    cmd_queue_head_bits[1]->cmd.funct),
        static_cast<unsigned>(cmd_queue_head_val[2]),
        static_cast<unsigned>(cmd_queue_head_bits[2]->rs_tag),
        commandName(cmd_queue_head_val[2] == 1,
                    cmd_queue_head_bits[2]->cmd.funct));

  // Check scratchpad request/response ordering and reject unexpected scratchpad writes.
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
    if (spad_write_val[bank] == 1 && spad_write_rdy[bank] == 1) {
      unexpected_spad_write_ = true;
      const auto& write = *spad_write_bits[bank];
      trace(ex_ctrl_write_mem_view, "spad bank=%u row=%u mask=0x%x\n",
            static_cast<unsigned>(bank), static_cast<unsigned>(write.addr),
            static_cast<unsigned>(write.mask));
    }
  }

  // These scenarios source operands from scratchpad, so accumulator reads are unexpected.
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    unexpected_accum_read_ |= accum_read_req_val[bank] == 1;
  }

  // Count accepted Mesher requests and verify their common WS control fields.
  if (mesher_req_val == 1 && mesher_req_rdy == 1) {
    const auto index = mesh_req_count_;
    mesh_ok_ &= index < test_.expected_mesh_requests.size();
    if (index < test_.expected_mesh_requests.size()) {
      mesh_ok_ &= matchesMesherRequest(*mesher_req_bits,
                                       test_.expected_mesh_requests[index]);
    }
    ++mesh_req_count_;
  }

  // A complete mesh row-beat enters only when all three physical inputs fire together.
  const bool a_fire = mesher_a_val == 1 && mesher_a_rdy == 1;
  const bool b_fire = mesher_b_val == 1 && mesher_b_rdy == 1;
  const bool d_fire = mesher_d_val == 1 && mesher_d_rdy == 1;
  const bool complete_input_row = a_fire && b_fire && d_fire;
  if ((a_fire || b_fire || d_fire) && !complete_input_row) {
    // Before the first memory response, only benign A/B fillers may arrive early.
    mesh_ok_ &= mesh_input_count_ == 0 && a_fire && b_fire && !d_fire &&
                mesher_a_bits->data == MeshInputRow{} &&
                mesher_b_bits->data == MeshInputRow{};
  }
  if (complete_input_row) {
    const auto index = mesh_input_count_;
    mesh_ok_ &= index < test_.expected_mesh_inputs.size();
    if (index < test_.expected_mesh_inputs.size()) {
      const auto& expected = test_.expected_mesh_inputs[index];
      mesh_ok_ &= mesher_a_bits->data == expected.a &&
                  mesher_b_bits->data == expected.b &&
                  mesher_d_bits->data == expected.d;
    }
    ++mesh_input_count_;
  }
  if (mesher_resp_val == 1) {
    const auto index = mesh_resp_count_;
    mesh_ok_ &= index < test_.expected_mesh_responses.size();
    if (index < test_.expected_mesh_responses.size()) {
      mesh_ok_ &= matchesMesherResponse(*mesher_resp_bits,
                                        test_.expected_mesh_responses[index]);
    }
    ++mesh_resp_count_;
  }

  // Compare each accepted accumulator write with the next golden output row.
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
                   write.data == expected.data && write.acc == 0 &&
                   write.mask == lowBitMask(kDim * sizeof(Acc));
      trace(ex_ctrl_write_mem_view,
            "acc  bank=%u row=%u data={%d,%d,%d,%d} mask=0x%x acc=%u\n",
            static_cast<unsigned>(bank), static_cast<unsigned>(write.addr),
            static_cast<int>(write.data[0]), static_cast<int>(write.data[1]),
            static_cast<int>(write.data[2]), static_cast<int>(write.data[3]),
            static_cast<unsigned>(write.mask),
            static_cast<unsigned>(write.acc));
    }
  }

  // Require completion tags to appear exactly once and in the declared sequence.
  if (completed_val == 1) {
    const auto tag = static_cast<SmeshRsTag>(*completed_bits);
    trace(ex_ctrl_completed_view, "tag=%u\n", static_cast<unsigned>(tag));

    if (next_completion_ >= test_.expected_completions.size() ||
        tag != test_.expected_completions[next_completion_]) {
      completion_ok_ = false;
    }
    ++next_completion_;

    if (std::find(test_.expected_mesh_completions.begin(),
                  test_.expected_mesh_completions.end(), tag) !=
        test_.expected_mesh_completions.end()) {
      completion_ok_ &= mesher_resp_val == 1 && mesher_resp_bits->last == 1 &&
                        mesher_resp_bits->tag.rs_tag_valid == 1 &&
                        mesher_resp_bits->tag.rs_tag == tag;
    }

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

  // Emit a compact cumulative progress line for cycle-by-cycle debugging.
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

// Restore all software-side counters and checker status before simulation starts.
void ExCtrlSuiteDriver::reset() {
  next_issue_ = 0;
  cmd_val.reset(0);
  cmd_bits.reset(SmeshIssue{});
  req_count_ = {};
  resp_count_ = {};
  mesh_req_count_ = 0;
  mesh_input_count_ = 0;
  mesh_resp_count_ = 0;
  write_count_ = 0;
  completion_count_.assign(test_.expected_completions.size(), 0);
  next_completion_ = 0;
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

// End the run only after every expected transaction has been observed.
bool ExCtrlSuiteDriver::activityComplete() const {
  bool complete = next_issue_ == test_.program.size() &&
                  mesh_req_count_ == test_.expected_progress.mesh_requests &&
                  mesh_input_count_ == test_.expected_progress.mesh_input_rows &&
                  mesh_resp_count_ == test_.expected_progress.mesh_output_rows &&
                  write_count_ == expected_writes_.size() &&
                  next_completion_ == test_.expected_completions.size();
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    complete &= req_count_[bank] == test_.expected_spad_reads[bank].size() &&
                resp_count_[bank] == test_.expected_spad_reads[bank].size();
  }
  for (const auto count : completion_count_) {
    complete &= count == 1;
  }
  return complete;
}

// A scenario passes only if it completed and every protocol/data check remained true.
bool ExCtrlSuiteDriver::passed() const {
  return activityComplete() && request_ok_ && response_ok_ && mesh_ok_ &&
         write_ok_ && completion_ok_ && !unexpected_accum_read_ &&
         !unexpected_spad_write_;
}

// Print a concise diagnosis when a scenario does not pass.
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

// Instantiate one independent ExCtrl test environment and connect all three components.
ExCtrlHarnessInstance::ExCtrlHarnessInstance(const ExCtrlTestCase& test,
                                             const std::string& prefix,
                                             Clock& clk) {
  ctrl_.reset(new ExCtrl(prefix + "ExCtrl"));
  driver_.reset(new ExCtrlSuiteDriver(test, prefix + "Driver"));
  spad_.reset(new ExCtrlSuiteSpad(test, prefix + "Spad"));

  // Connect command input, completion output, and test-only observability taps.
  ctrl_->cmd_val << driver_->cmd_val;
  ctrl_->cmd_bits << driver_->cmd_bits;
  driver_->cmd_rdy << ctrl_->cmd_rdy;
  driver_->completed_val << ctrl_->completed_val;
  driver_->completed_bits << ctrl_->completed_bits;
  driver_->control_state << ctrl_->control_state;
  for (std::size_t slot = 0; slot < kExCtrlCmdWindow; ++slot) {
    driver_->cmd_queue_head_val[slot] << ctrl_->cmd_queue_head_val[slot];
    driver_->cmd_queue_head_bits[slot] << ctrl_->cmd_queue_head_bits[slot];
  }

  // Observe Mesher request, input-row, and response activity exposed by ExCtrl.
  driver_->mesher_req_val << ctrl_->mesher_req_val;
  driver_->mesher_req_rdy << ctrl_->mesher_req_rdy;
  driver_->mesher_req_bits << ctrl_->mesher_req_bits;
  driver_->mesher_a_val << ctrl_->mesher_a_val;
  driver_->mesher_a_rdy << ctrl_->mesher_a_rdy;
  driver_->mesher_a_bits << ctrl_->mesher_a_bits;
  driver_->mesher_b_val << ctrl_->mesher_b_val;
  driver_->mesher_b_rdy << ctrl_->mesher_b_rdy;
  driver_->mesher_b_bits << ctrl_->mesher_b_bits;
  driver_->mesher_d_val << ctrl_->mesher_d_val;
  driver_->mesher_d_rdy << ctrl_->mesher_d_rdy;
  driver_->mesher_d_bits << ctrl_->mesher_d_bits;
  driver_->mesher_resp_val << ctrl_->mesher_resp_val;
  driver_->mesher_resp_bits << ctrl_->mesher_resp_bits;

  // Put the scratchpad model on ExCtrl's read ports and mirror traffic into the checker.
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
    driver_->spad_write_bits[bank] << ctrl_->spad_write_bits[bank];
  }

  // Keep accumulator reads empty while accepting and checking accumulator writes.
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    ctrl_->accum_read_req_rdy[bank] << driver_->accum_read_req_rdy[bank];
    driver_->accum_read_req_val[bank] << ctrl_->accum_read_req_val[bank];
    ctrl_->accum_read_resp_val[bank] << driver_->accum_read_resp_val[bank];
    ctrl_->accum_read_resp_bits[bank] << driver_->accum_read_resp_bits[bank];
    ctrl_->accum_write_rdy[bank] << driver_->accum_write_rdy[bank];
    driver_->accum_write_val[bank] << ctrl_->accum_write_val[bank];
    driver_->accum_write_bits[bank] << ctrl_->accum_write_bits[bank];
  }

  // All components share one clock.
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
