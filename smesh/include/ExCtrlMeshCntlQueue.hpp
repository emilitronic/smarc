// **********************************************************************
// smesh/include/ExCtrlMeshCntlQueue.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
/*
Mesh-control metadata queue for ExecuteController row-beats.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshLocalAddr.hpp"
#include "SmeshPorts.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace smesh {

struct ExCtrlMeshCntl {
  bit perform_mul_pre = 0;
  bit perform_single_mul = 0;
  bit perform_single_preload = 0;
  std::uint32_t a_bank = 0;
  std::uint32_t b_bank = 0;
  std::uint32_t d_bank = 0;
  std::uint32_t a_bank_acc = 0;
  std::uint32_t b_bank_acc = 0;
  std::uint32_t d_bank_acc = 0;
  bit a_read_from_acc = 0;
  bit b_read_from_acc = 0;
  bit d_read_from_acc = 0;
  bit a_garbage = 0;
  bit b_garbage = 0;
  bit d_garbage = 0;
  bit accumulate_zeros = 0;
  bit preload_zeros = 0;
  bit a_fire = 0;
  bit b_fire = 0;
  bit d_fire = 0;
  std::uint32_t a_unpadded_cols = 0; 
  std::uint32_t b_unpadded_cols = 0;
  std::uint32_t d_unpadded_cols = 0;

  u8             dataflow     = 0;  // req fields to Mesher 
  bit            prop         = 0;
  u8             shift        = 0;
  bit            a_transpose  = 0;
  bit            bd_transpose = 0;
  std::uint32_t  total_rows   = 0;
  bit            rs_tag_valid = 0;
  SmeshRsTag     rs_tag       = 0;
  SmeshLocalAddr c_addr{};
  std::uint32_t  c_rows = 0;
  std::uint32_t  c_cols = 0;

  bit            im2colling = 0;
  bit            first = 0;
};

class ExCtrlMeshCntlQueue : public Component {
  DECLARE_COMPONENT(ExCtrlMeshCntlQueue, MQ);

 public:
  ExCtrlMeshCntlQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,            enq_val);
  Output(bit,           enq_rdy);
  Input(ExCtrlMeshCntl, enq_bits);

  Output(bit,            cntl_val);
  Input(bit,             mesh_cntl_deq_rdy);
  Output(ExCtrlMeshCntl, cntl_bits);
  Output(ExCtrlMeshReq,  mesh_req_bits);

  void updateEnqReady();
  void updateDeqView();
  void updateStorage();
  void reset();

 private:
  // Hold control metadata until the corresponding fixed-latency SPAD data returns.
  static constexpr std::size_t kDepth = kSpadReadDelay + 1;

  std::array<ExCtrlMeshCntl, kDepth> entries_{};
  std::size_t head_  = 0;
  std::size_t tail_  = 0;
  std::size_t count_ = 0;
};

} // namespace smesh
