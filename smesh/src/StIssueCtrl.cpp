// **********************************************************************
// smesh/src/StIssueCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 14 2026
/*
Store-path final issue control skeleton implementation.
*/

#include "StIssueCtrl.hpp"

namespace smesh {

namespace {

enum : std::uint8_t {
  kDataSourceZero = 0,
  kDataSourceSpad = 1,
  kDataSourceAcc  = 2,
};

enum : std::uint8_t {
  kWriterDma  = 0,
  kWriterSpad = 1,
};

enum : std::uint8_t {
  kFinalDataZero         = 0,
  kFinalDataNormalWidth  = 1,
  kFinalDataFullAccWidth = 2,
};

struct IssueDecision {
  bool wi_valid       = false;
  bool garbage        = false;
  bool is_acc         = false;
  bool full_width     = false;
  bool dest_spad      = false;
  bool use_zero       = false;
  bool use_spad_data  = false;
  bool use_acc_data   = false;
  bool data_available = false;
  bool writer_ready   = false;
};
// classification helper, bundle a bunch of simple decisions
IssueDecision classifyIssue(const DmaWriteReq& issue,
                            bool issue_valid,
                            bool spad_data_valid,
                            bool acc_data_valid,
                            bool dma_writer_ready,
                            bool spad_writer_ready) {
  IssueDecision d{};
  const auto laddr = issue.laddr;
  d.wi_valid       = issue_valid;
  d.garbage        = laddr.is_garbage();
  d.is_acc         = laddr.is_acc_addr();
  d.full_width     = laddr.read_full_acc_row();
  d.dest_spad      = issue.dest != 0;
  d.use_zero       = d.garbage;
  d.use_spad_data  = !d.garbage && !d.is_acc;
  d.use_acc_data   = !d.garbage && d.is_acc;
  d.data_available = d.use_zero || (d.use_spad_data && spad_data_valid) || (d.use_acc_data && acc_data_valid);
  d.writer_ready   = d.dest_spad ? spad_writer_ready : dma_writer_ready;
  return d;
}

} // namespace

StIssueCtrl::StIssueCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateDataOutputs).reads(issue_deq_val,
                                  issue_deq_bits,
                                  dma_writer_req_rdy,
                                  spad_writer_req_rdy,
                                  spad_data_val,
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
// selector/data outputs
void StIssueCtrl::updateDataOutputs() {
  const DmaWriteReq issue = *issue_deq_bits; // always ingest, but we check valids right below, so all good
  const auto spad_bank = issue.laddr.sp_bank();
  const bool selected_spad_data_val = spad_data_val[spad_bank] != 0;
  // compute a bundle of useful booleans
  const auto d = classifyIssue(issue, issue_deq_val != 0,selected_spad_data_val,acc_data_val != 0,dma_writer_req_rdy != 0,spad_writer_req_rdy != 0);
  // default scenario A) a zero-data write
  std::uint8_t next_data_source = kDataSourceZero;
  std::uint8_t next_final_data  = kFinalDataZero;
  if (d.use_spad_data) {       // scenario B) a normal-width write from SpadDmaReadPipe
    next_data_source = kDataSourceSpad;
    next_final_data  = kFinalDataNormalWidth;
  } else if (d.use_acc_data) { // scenario C) a normal-width or full-width write from AccScaleUnit
    next_data_source = kDataSourceAcc;
    next_final_data  = d.full_width ? kFinalDataFullAccWidth : kFinalDataNormalWidth;
  }

  const bool issue_fire    = d.wi_valid && d.data_available && d.writer_ready; // selected writer accepts one store req this cycle
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_data_rdy[bank] = bit(issue_fire && d.use_spad_data && bank == spad_bank); // consume only the selected bank's spad item
  }
  acc_data_rdy             = bit(issue_fire && d.use_acc_data);  // if issue fires and acc is source, consume acc item
  data_source_sel          = u8(next_data_source); // where to take candidate data from: ZERO, SPAD_DMA_PIPE, or ACC_SCALE_UNIT
  final_data_sel           = u8(next_final_data);  // which payload is sent to writer:   ZERO, NORMAL_WIDTH, or FULL_ACC_WIDTH
  write_data_is_all_zeros  = bit(d.garbage);       // if garbage, mark data as all zeros
  write_data_is_full_width = bit(!d.garbage && d.is_acc && d.full_width); // write full-width activation elements
}
// writer/metadata outputs
void StIssueCtrl::updateWriterOutputs() {
  const DmaWriteReq issue = *issue_deq_bits;
  const auto spad_bank = issue.laddr.sp_bank();
  const bool selected_spad_data_val = spad_data_val[spad_bank] != 0;
  const auto d = classifyIssue(issue,issue_deq_val != 0,selected_spad_data_val,acc_data_val != 0,dma_writer_req_rdy != 0,spad_writer_req_rdy != 0);

  const bool issue_fire   = d.wi_valid && d.data_available && d.writer_ready;
  dma_writer_req_val      = bit(d.wi_valid && d.data_available && !d.dest_spad); // tell DMA writer is has valid req
  spad_writer_req_val     = bit(d.wi_valid && d.data_available && d.dest_spad);  // tell SPAD writer is has valid req
  issue_deq_rdy           = bit(issue_fire);                                     // pop metadata from issue queue if writer accepts this cycle
  writer_sel              = u8(d.dest_spad ? kWriterSpad : kWriterDma);          // which writer path is chosen (not used at moment)
}

} // namespace smesh
