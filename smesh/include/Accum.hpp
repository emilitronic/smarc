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

  FifoOutput(DmaReadCompletion, dma_resp); // completion FIFO: let LdCtrl know last accum write is done
  // Banked write ports. For now Accum accepts at most one write per cycle.
  InputArray(bit, write_val_bnk, kAccBanks);
  OutputArray(bit, write_rdy_bnk, kAccBanks);
  InputArray(DmaReadResp, write_bits_bnk, kAccBanks);

  // Banked read request ports. For now Accum accepts at most one read per cycle.
  // TODO: ready is currently advertised on every bank even though updateRead()
  // consumes only the first valid request. Use per-bank response state, or make
  // ready one-hot, so concurrent handshakes cannot be silently dropped.
  InputArray(bit, read_req_val_bnk, kAccBanks);
  OutputArray(bit, read_req_rdy_bnk, kAccBanks);
  InputArray(AccumReadReq, read_req_bits_bnk, kAccBanks);

  OutputArray(bit, read_resp_val_bnk, kAccBanks);
  OutputArray(AccumReadResp, read_resp_bits_bnk, kAccBanks);
  InputArray(bit, read_resp_rdy_bnk, kAccBanks);

  void updateWriteReady();
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
