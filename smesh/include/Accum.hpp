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
  FifoInput(AccumReadReq, read_req);       // accum's read req port
  Output(bit, read_rdy);                   // tap out read req ready signal for StReadCtrl to inspect
  FifoOutput(AccumReadResp, read_resp);

  void updateWrite();
  void updateReadReady();
  void updateRead();
  void reset();

  bool hasAcceptedWrite() const { return write_accepted_; }
  const Row& row(SmeshLocalAddr addr) const;

 private:
  std::array<std::array<Row, kAccBankRows>, kAccBanks> banks_{};
  bool write_accepted_ = false;
};

} // namespace smesh
