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
  // Each bank accepts writes independently; same-bank reads may be excluded.
  InputArray(bit, write_val_bnk, kSpBanks);
  OutputArray(bit, write_rdy_bnk, kSpBanks);
  InputArray(DmaReadResp, write_bits_bnk, kSpBanks);

  // Every bank has an independent read request and one-entry response slot.
  InputArray(bit, read_req_val_bnk, kSpBanks);
  OutputArray(bit, read_req_rdy_bnk, kSpBanks);
  InputArray(SpadBankReadReq, read_req_bits_bnk, kSpBanks);

  OutputArray(bit, read_resp_val_bnk, kSpBanks);
  OutputArray(SpadReadResp, read_resp_bits_bnk, kSpBanks);
  InputArray(bit, read_resp_rdy_bnk, kSpBanks);

  void updateWriteReady();
  void updateWrite();
  void updateReadReady();
  void updateReadRespView();
  void updateRead();
  void reset();

  bool hasAcceptedWrite() const { return write_accepted_; }
  const Row& row(SmeshLocalAddr addr) const;

 private:
  std::array<std::array<Row, kSpBankRows>, kSpBanks> banks_{};
  bool write_accepted_ = false;
  OutputArray(bit, read_resp_valid_Q_, kSpBanks); // response held per bank this cycle
  OutputArray(SpadReadResp, read_resp_entry_Q_, kSpBanks);
  RegisterArray(bit, read_resp_valid_D_, kSpBanks); // response state for next cycle
  RegisterArray(SpadReadResp, read_resp_entry_D_, kSpBanks);
};

} // namespace smesh
