// **********************************************************************
// smesh/include/StIssueCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 14 2026
/*
Store-path final issue control skeleton.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class StIssueCtrl : public Component {
  DECLARE_COMPONENT(StIssueCtrl);

 public:
  StIssueCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, issue_deq_val);        // DmaWriteIssueQueue head metadata is valid
  Input(DmaWriteReq, issue_deq_bits); // DmaWriteIssueQueue head metadata payload

  Input(bit, dma_writer_req_rdy);   // DmaWriter can accept selected metadata/data this cycle
  Input(bit, spad_writer_req_rdy);  // SpadWriter can accept selected metadata/data this cycle

  Input(bit, spad_data_val);        // SpadDmaReadPipe has normal-width data available
  Input(SpadReadResp, spad_data_bits); // SpadDmaReadPipe data payload

  Input(bit, acc_data_val);         // AccScaleUnit has accumulator-sourced data available
  Input(AccScaleResp, acc_data_bits); // AccScaleUnit data payload

  Output(bit, spad_data_rdy); // tells SpadDmaReadPipe output it may be consumed
  Output(bit, acc_data_rdy);  // tells AccScaleUnit output it may be consumed

  Output(bit, dma_writer_req_val);  // tells DmaWriter this cycle's selected metadata/data is valid
  Output(bit, spad_writer_req_val); // tells SpadWriter this cycle's selected metadata/data is valid

  Output(bit, issue_deq_rdy); // tells DmaWriteIssueQueue its head metadata may pop

  Output(u8, data_source_sel); // selects ZERO, SPAD_DMA_PIPE, or ACC_SCALE_UNIT as write data source
  Output(u8, writer_sel);      // selects NORMAL_WRITER or SPAD_WRITER as write destination
  Output(u8, final_data_sel);  // selects ZERO, NORMAL_WIDTH, or FULL_ACC_WIDTH as final writer data

  Output(bit, write_data_is_all_zeros); // marks selected write data as all zeros
  Output(bit, write_data_is_full_width); // marks selected write data as full accumulator width

  void updateDataOutputs();
  void updateWriterOutputs();
};

} // namespace smesh
