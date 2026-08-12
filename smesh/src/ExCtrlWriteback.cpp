// **********************************************************************
// smesh/src/ExCtrlWriteback.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 30 2026

#include "ExCtrlWriteback.hpp"

namespace smesh {

ExCtrlWriteback::ExCtrlWriteback(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateView)
      .reads(mesh_resp_val,
             mesh_resp_bits,
             current_dataflow,
             c_addr_stride,
             activation,
             aligned_to,
             ex_write_to_spad,
             ex_write_to_acc)
      .reads(spad_write_rdy,
             accum_write_rdy)
      .writes(spad_write_val,
              spad_write_bits,
              accum_write_val,
              accum_write_bits,
              output_counter,
              start_array_outputting)
      .writes(mesh_completed_rs_tag_fire,
              completed_val,
              completed_bits);
  UPDATE(updateState)
      .reads(mesh_resp_val, mesh_resp_bits);
}

void ExCtrlWriteback::updateView() {
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_write_val[bank] = 0;
    spad_write_bits[bank] = DmaReadResp{};
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_write_val[bank] = 0;
    accum_write_bits[bank] = DmaReadResp{};
  }

  output_counter = output_counter_;
  start_array_outputting = 0;
  mesh_completed_rs_tag_fire = 0;
  completed_val = 0;
  completed_bits = 0;
}

void ExCtrlWriteback::updateState() {
  const bool mesh_resp_fire = mesh_resp_val != 0;

  // TODO: advance/reset output_counter_ when mesh output routing is implemented.
  if (mesh_resp_fire && static_cast<bool>(mesh_resp_bits->last)) {
    output_counter_ = 0;
  }
}

void ExCtrlWriteback::reset() {
  output_counter_ = 0;

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_write_val[bank].reset(0);
    spad_write_bits[bank].reset(DmaReadResp{});
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_write_val[bank].reset(0);
    accum_write_bits[bank].reset(DmaReadResp{});
  }
  output_counter.reset(0);
  start_array_outputting.reset(0);
  mesh_completed_rs_tag_fire.reset(0);
  completed_val.reset(0);
  completed_bits.reset(0);
}

} // namespace smesh
