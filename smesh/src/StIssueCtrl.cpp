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
  UPDATE(updateDataOutputs).reads(issue_deq_val,
                                  issue_deq_bits,
                                  spad_data_val,
                                  spad_data_bits,
                                  acc_data_val,
                                  acc_data_bits)
                           .writes(spad_data_rdy,
                                   acc_data_rdy,
                                   data_source_sel,
                                   final_data_sel,
                                   write_data_is_all_zeros,
                                   write_data_is_full_width);
  UPDATE(updateWriterOutputs).reads(issue_deq_val,
                                    issue_deq_bits,
                                    dma_writer_req_rdy,
                                    spad_writer_req_rdy,
                                    spad_data_val,
                                    acc_data_val)
                             .writes(dma_writer_req_val,
                                     spad_writer_req_val,
                                     issue_deq_rdy,
                                     writer_sel);
}

void StIssueCtrl::updateDataOutputs() {
  const auto issue      = *issue_deq_bits;
  const auto laddr      = issue.laddr;
  const bool garbage    = laddr.is_garbage();
  const bool is_acc     = laddr.is_acc_addr();
  const bool full_width = laddr.read_full_acc_row();

  spad_data_rdy            = bit(false);
  acc_data_rdy             = bit(false);
  data_source_sel          = u8(0);
  final_data_sel           = u8(0);
  write_data_is_all_zeros  = bit(garbage); // if garbage, mark data as all zeros
  write_data_is_full_width = bit(!garbage && is_acc && full_width); // write full-width activation elements
}

void StIssueCtrl::updateWriterOutputs() {
  dma_writer_req_val  = bit(false);
  spad_writer_req_val = bit(false);
  issue_deq_rdy       = bit(false);
  writer_sel          = u8(0);
}

} // namespace smesh
