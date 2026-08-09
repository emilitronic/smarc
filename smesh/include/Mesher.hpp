// **********************************************************************
// smesh/include/Mesher.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
/*
Systolic mesh wrapper.  Manages the interface between ExCtrl and the MeshHull.
This includes request tracking, row-beat counting, and tag/total_rows bookkeeping.
*/

#pragma once

#include <array>
#include <cstdint>

#include <cascade/Cascade.hpp>

#include "MeshHull.hpp"
#include "SmeshPorts.hpp"
#include "SmeshTypes.hpp"

namespace smesh {

class Mesher : public Component {
  DECLARE_COMPONENT(Mesher);

 public:
  Mesher(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,           req_val);
  Output(bit,          req_rdy);
  Input(ExCtrlMeshReq, req_bits);

  Input(bit,          a_val);
  Output(bit,         a_rdy);
  Input(ExCtrlMeshIn, a_bits);

  Input(bit,          b_val);
  Output(bit,         b_rdy);
  Input(ExCtrlMeshIn, b_bits);

  Input(bit,          d_val);
  Output(bit,         d_rdy);
  Input(ExCtrlMeshIn, d_bits);

  Output(bit,          transposer_in_row_val);
  Output(MeshInputRow, transposer_in_row_bits);
  Input(MeshInputRow,  transposer_out_col_bits);

  Output(bit,        resp_val);
  Output(MesherResp, resp_bits);

  OutputArray(MesherTag, tags_in_progress, kRsExecuteEntries);

  void update();
  void reset();

 private:
  static constexpr std::size_t kTagQueueEntries = kMaxSimultaneousMatmuls + 1;

  struct TagQEntry {
    ExCtrlMeshTag tag{};
    std::uint8_t  id = 0;
  };

  struct TotalRowsQEntry {
    std::uint32_t total_rows = 0;
    std::uint8_t  id         = 0;
  };

  ExCtrlMeshReq req_state_{};             // holds current request being processed
  bool          req_state_valid_ = false; // true if req_state_ is valid and being processed
  std::uint8_t  matmul_id_       = 0;     // id attached to rows entering the mesh for the active request
  std::uint8_t  next_matmul_id_  = 0;     // next mesh-local id assigned on request fire
  bool          in_prop_         = false; // registered propagate control loaded from req_bits on request fire
  bool          a_written_       = false; // true once A input for current row-beat has been accepted
  bool          b_written_       = false; // true once B input for current row-beat has been accepted
  bool          d_written_       = false; // true once D input for current row-beat has been accepted
  std::uint32_t fire_counter_    = 0;     // row-beats advanced into the mesh for current request

  std::array<TagQEntry, kTagQueueEntries>       tagq_{};             // tags indexed by mesh-local output id
  std::uint8_t                                  tagq_head_  = 0;
  std::uint8_t                                  tagq_tail_  = 0;
  std::uint8_t                                  tagq_count_ = 0;
  std::array<TotalRowsQEntry, kTagQueueEntries> total_rows_q_{};     // total rows indexed by current matmul id
  std::uint8_t                                  total_rows_q_head_  = 0;
  std::uint8_t                                  total_rows_q_tail_  = 0;
  std::uint8_t                                  total_rows_q_count_ = 0;

  MeshHull hull_{};
};

} // namespace smesh
