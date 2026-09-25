// **********************************************************************
// smesh/include/controllers/ld/LdCtrlGeom.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026
/*
Computes the current virtual and local row addresses, detects vaddr == 0, and 
reduces actual_rows_read to 1 for a nonzero-address, zero-stride load.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshLocalAddr.hpp"

namespace smesh {

// Computes the current load row's addresses and effective row count.
class LdCtrlGeom : public Component {
  DECLARE_COMPONENT(LdCtrlGeom);

 public:
  LdCtrlGeom(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(u64,            vaddr);
  Input(SmeshLocalAddr, localaddr);
  Input(u32,            rows);
  Input(u64,            stride);
  Input(u32,            row_counter);

  Output(bit,            all_zeros);
  Output(u64,            current_vaddr);
  Output(SmeshLocalAddr, localaddr_plus_row_counter);
  Output(u32,            actual_rows_read);

  void update();
};

} // namespace smesh
