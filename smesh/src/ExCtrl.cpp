// **********************************************************************
// smesh/src/ExCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026

#include "ExCtrl.hpp"

namespace smesh {

ExCtrl::ExCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReadPorts).writes(spad_read_req_val,
                                 spad_read_req_bits,
                                 spad_read_resp_rdy,
                                 accum_read_req_val,
                                 accum_read_req_bits,
                                 accum_read_resp_rdy);
  UPDATE(updateWritePorts).writes(spad_write_val,
                                  spad_write_bits,
                                  accum_write_val,
                                  accum_write_bits);
}

void ExCtrl::updateReadPorts() {
  spad_read_req_val = 0;
  spad_read_req_bits = SpadReadReq{};
  spad_read_resp_rdy = 0;

  accum_read_req_val = 0;
  accum_read_req_bits = AccumReadReq{};
  accum_read_resp_rdy = 0;
}

void ExCtrl::updateWritePorts() {
  spad_write_val = 0;
  spad_write_bits = DmaReadResp{};

  accum_write_val = 0;
  accum_write_bits = DmaReadResp{};
}

void ExCtrl::reset() {
  spad_read_req_val.reset(0);
  spad_read_req_bits.reset(SpadReadReq{});
  spad_read_resp_rdy.reset(0);

  accum_read_req_val.reset(0);
  accum_read_req_bits.reset(AccumReadReq{});
  accum_read_resp_rdy.reset(0);

  spad_write_val.reset(0);
  spad_write_bits.reset(DmaReadResp{});

  accum_write_val.reset(0);
  accum_write_bits.reset(DmaReadResp{});
}

} // namespace smesh
