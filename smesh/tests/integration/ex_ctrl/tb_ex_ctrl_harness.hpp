// **********************************************************************
// smesh/tests/integration/ex_ctrl/tb_ex_ctrl_harness.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 17 2026
/*
Reusable ExCtrl end-to-end driver, local-memory model, and checker.  
Contains the reusable simulated environment surrounding ExCtrl.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "ExCtrl.hpp"
#include "tb_ex_ctrl_dsl.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace smesh {
namespace tb {

struct ExCtrlSuiteSpadPipeline {
  std::array<std::array<bit, kSpadReadDelay>, kSpBanks> valid{};
  std::array<std::array<SpadReadResp, kSpadReadDelay>, kSpBanks> bits{};
};

// Owns and models a single SPAD memory instance for the ExCtrl test harness.
class ExCtrlSuiteSpad : public Component {
  DECLARE_COMPONENT(ExCtrlSuiteSpad, SpadModel);

 public:
  ExCtrlSuiteSpad(const ExCtrlTestCase& test, std::string name, COMPONENT_CTOR);

  Clock(clk);
  InputArray(bit, req_val, kSpBanks);
  OutputArray(bit, req_rdy, kSpBanks);
  InputArray(SpadBankReadReq, req_bits, kSpBanks);
  OutputArray(bit, resp_val, kSpBanks);
  InputArray(bit, resp_rdy, kSpBanks);
  OutputArray(SpadReadResp, resp_bits, kSpBanks);

  void updateRespView();
  void updateReady();
  void updateNextState();
  void reset();

 private:
  const ExCtrlTestCase& test_;
  std::array<MeshInputRow, kSpRows> image_{};
  std::array<bool, kSpRows> initialized_{};

  Output(bit, active_Q_);
  Register(bit, active_D_);
  Output(ExCtrlSuiteSpadPipeline, pipeline_Q_);
  Register(ExCtrlSuiteSpadPipeline, pipeline_D_);
};

struct ExpectedAccumWrite {
  std::string name;
  std::uint32_t address = 0;
  MeshAccumRow data{};
};

// ***********************
// ExCtrl Suite Driver
// 1) Sends commands into ExCtrl
// 2) Models surrounding memory environment (SPAD and Accum)
// 3) Monitors and validates ExCtrl
class ExCtrlSuiteDriver : public Component {
  DECLARE_COMPONENT(ExCtrlSuiteDriver, Driver);

 public:
  ExCtrlSuiteDriver(const ExCtrlTestCase& test, std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoOutput(SmeshIssue, cmd_out);
  Input(bit, completed_val);
  Input(SmeshRsTag, completed_bits);

  // Test-only ExCtrl taps used by the command-window trace.
  Input(u8, control_state);
  InputArray(bit, cmd_queue_head_val, kExCtrlCmdWindow);
  InputArray(SmeshIssue, cmd_queue_head_bits, kExCtrlCmdWindow);

  Input(bit, mesher_req_val);
  Input(bit, mesher_req_rdy);
  Input(ExCtrlMeshReq, mesher_req_bits);
  Input(bit, mesher_a_val);
  Input(bit, mesher_a_rdy);
  Input(ExCtrlMeshIn, mesher_a_bits);
  Input(bit, mesher_b_val);
  Input(bit, mesher_b_rdy);
  Input(ExCtrlMeshIn, mesher_b_bits);
  Input(bit, mesher_d_val);
  Input(bit, mesher_d_rdy);
  Input(ExCtrlMeshIn, mesher_d_bits);
  Input(bit, mesher_resp_val);
  Input(MesherResp, mesher_resp_bits);

  InputArray(bit, spad_read_req_val, kSpBanks);
  InputArray(bit, spad_read_req_rdy, kSpBanks);
  InputArray(SpadBankReadReq, spad_read_req_bits, kSpBanks);
  InputArray(bit, spad_read_resp_val, kSpBanks);
  InputArray(bit, spad_read_resp_rdy, kSpBanks);
  InputArray(SpadReadResp, spad_read_resp_bits, kSpBanks);

  OutputArray(bit, accum_read_req_rdy, kAccBanks);
  InputArray(bit, accum_read_req_val, kAccBanks);
  OutputArray(bit, accum_read_resp_val, kAccBanks);
  OutputArray(ExCtrlAccumReadResp, accum_read_resp_bits, kAccBanks);

  OutputArray(bit, spad_write_rdy, kSpBanks);
  InputArray(bit, spad_write_val, kSpBanks);
  InputArray(SpadBankWriteReq, spad_write_bits, kSpBanks);
  OutputArray(bit, accum_write_rdy, kAccBanks);
  InputArray(bit, accum_write_val, kAccBanks);
  InputArray(AccumBankWriteReq, accum_write_bits, kAccBanks);

  void updateIssue();
  void updateMemoryReady();
  void updateMonitor();
  void reset();

  bool activityComplete() const;
  bool passed() const;
  void report() const;

 private:
  const ExCtrlTestCase& test_;
  std::array<MeshInputRow, kSpRows> spad_image_{};
  std::array<bool, kSpRows> spad_initialized_{};
  std::vector<ExpectedAccumWrite> expected_writes_;

  std::size_t next_issue_ = 0;
  std::array<std::size_t, kSpBanks> req_count_{};
  std::array<std::size_t, kSpBanks> resp_count_{};
  std::size_t mesh_req_count_ = 0;
  std::size_t mesh_input_count_ = 0;
  std::size_t mesh_resp_count_ = 0;
  std::size_t write_count_ = 0;
  std::vector<std::size_t> completion_count_;
  std::size_t next_completion_ = 0;

  bool request_ok_ = true;
  bool response_ok_ = true;
  bool mesh_ok_ = true;
  bool write_ok_ = true;
  bool completion_ok_ = true;
  bool unexpected_accum_read_ = false;
  bool unexpected_spad_write_ = false;
};
// ExCtrl Suite Driver
// ***********************


// Owns and wires one independent ExCtrl simulation instance.
// Each ExCtrlHarnessInstance contains its own ExCtrl, command driver/checker, and SPAD model.
class ExCtrlHarnessInstance {
 public:
  ExCtrlHarnessInstance(const ExCtrlTestCase& test, const std::string& prefix, Clock& clk);

  bool activityComplete() const;
  bool passed() const;
  void report() const;

 private:
  std::unique_ptr<ExCtrl> ctrl_;
  std::unique_ptr<ExCtrlSuiteDriver> driver_;
  std::unique_ptr<ExCtrlSuiteSpad> spad_;
};

} // namespace tb
} // namespace smesh
