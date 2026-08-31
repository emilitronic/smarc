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

  FifoOutput(DmaReadCompletion, dma_resp); // completion FIFO: let LdCtrl know last spad write is done
  // Banked write ports. For now Spad accepts at most one write per cycle.
  InputArray(bit, write_val_bnk, kSpBanks);
  OutputArray(bit, write_rdy_bnk, kSpBanks);
  InputArray(DmaReadResp, write_bits_bnk, kSpBanks);

  // Banked read request ports. For now Spad accepts at most one read per cycle.
  // TODO: ready is currently advertised on every bank even though updateRead()
  // consumes only the first valid request. Use per-bank response state, or make
  // ready one-hot, so concurrent handshakes cannot be silently dropped.
  InputArray(bit, read_req_val_bnk, kSpBanks);
  OutputArray(bit, read_req_rdy_bnk, kSpBanks);
  InputArray(SpadReadReq, read_req_bits_bnk, kSpBanks);

  OutputArray(bit, read_resp_val_bnk, kSpBanks);
  OutputArray(SpadReadResp, read_resp_bits_bnk, kSpBanks);
  InputArray(bit, read_resp_rdy_bnk, kSpBanks);

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
  std::array<std::array<Row, kSpBankRows>, kSpBanks> banks_{};
  bool write_accepted_ = false;
  bool read_resp_valid_ = false;   // reg holds response valid while waiting for read pipe to pop it
  SpadReadResp read_resp_entry_{}; // reg holds response while waiting for read pipe to pop it
};

} // namespace smesh
