// **********************************************************************
// smesh/src/StIssueCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 14 2026
/*
Store-path final issue control skeleton implementation.
*/

#include "StIssueCtrl.hpp"

namespace smesh {

StIssueCtrl::StIssueCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateDataOutputs).writes(spad_data_rdy,
                                   acc_data_rdy,
                                   data_source_sel,
                                   final_data_sel,
                                   write_data_is_all_zeros,
                                   write_data_is_full_width);
  UPDATE(updateWriterOutputs).writes(dma_writer_req_val,
                                     spad_writer_req_val,
                                     issue_deq_rdy,
                                     writer_sel);
}

void StIssueCtrl::updateDataOutputs() {
  spad_data_rdy = bit(false);
  acc_data_rdy = bit(false);
  data_source_sel = u8(0);
  final_data_sel = u8(0);
  write_data_is_all_zeros = bit(false);
  write_data_is_full_width = bit(false);
}

void StIssueCtrl::updateWriterOutputs() {
  dma_writer_req_val = bit(false);
  spad_writer_req_val = bit(false);
  issue_deq_rdy = bit(false);
  writer_sel = u8(0);
}

} // namespace smesh
