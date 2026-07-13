// **********************************************************************
// smesh/include/Accum.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 9 2026
/*
Standalone smesh accumulator memory. Initially provides one normal-width load-path write port.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"
#include "SmeshTypes.hpp"

#include <array>

namespace smesh {

class Accum : public Component {
  DECLARE_COMPONENT(Accum);

 public:
  using Row = std::array<Acc, kDim>;

  Accum(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(DmaReadResp, write_in);        // accum's write port
  FifoOutput(DmaReadCompletion, dma_resp); // completion FIFO: let LdCtrl know last accum write is done

  Input(bit, read_req_val);                // accum's read req valid signal
  Output(bit, read_req_rdy);               // tap out read req ready signal for StReadCtrl to inspect
  Input(AccumReadReq, read_req_bits);      // accum's read req payload

  Output(bit, read_resp_val);
  Output(AccumReadResp, read_resp_bits);
  Input(bit, read_resp_rdy);

  void updateWrite();
  void updateReadReady();
  void updateReadRespView();
  void updateReadRespPop();
  void updateRead();
  void reset();

  bool hasAcceptedWrite() const { return write_accepted_; }
  const Row& row(SmeshLocalAddr addr) const;

 private:
  std::array<std::array<Row, kAccBankRows>, kAccBanks> banks_{};
  bool write_accepted_  = false;
  bool read_resp_valid_ = false;
  AccumReadResp read_resp_entry_{}; // reg holds response while waiting for StNormCtrl to pop it
};

} // namespace smesh
