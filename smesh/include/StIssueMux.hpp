// **********************************************************************
// smesh/include/StIssueMux.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 14 2026
/*
Store-path final issue payload mux.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class StIssueMux : public Component {
  DECLARE_COMPONENT(StIssueMux);

 public:
  StIssueMux(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(DmaWriteReq, issue_bits);
  InputArray(SpadReadResp, spad_data_bits, kSpBanks);
  Input(AccScaleResp, acc_data_bits);

  Input(u8, data_source_sel);
  Input(u8, final_data_sel);
  Input(bit, write_data_is_all_zeros);
  Input(bit, write_data_is_full_width);

  Output(StWriterReq, writer_req_bits);

  void update();
};

} // namespace smesh
