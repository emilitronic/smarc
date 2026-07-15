// **********************************************************************
// smesh/include/Spad.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Standalone smesh scratchpad memory. Initially provides one load-path write port.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"
#include "SmeshTypes.hpp"

#include <array>

namespace smesh {

class Spad : public Component {
  DECLARE_COMPONENT(Spad);

 public:
  using Row = std::array<Elem, kDim>;

  Spad(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(DmaReadResp, write_in);        // spad's write port
  FifoOutput(DmaReadCompletion, dma_resp); // completion FIFO: let LdCtrl know last spad write is done
  Input(bit, read_req_val);                // spad's read req valid signal
  Output(bit, read_req_rdy);               // tap out read req ready signal for StReadCtrl to inspect
  Input(SpadReadReq, read_req_bits);       // spad's read req payload
  Output(bit, read_resp_val);
  Output(SpadReadResp, read_resp_bits);
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
  std::array<std::array<Row, kSpBankRows>, kSpBanks> banks_{};
  bool write_accepted_ = false;
  bool read_resp_valid_ = false;
  SpadReadResp read_resp_entry_{}; // reg holds response while waiting for read pipe to pop it
};

} // namespace smesh
