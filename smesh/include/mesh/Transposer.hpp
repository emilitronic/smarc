// **********************************************************************
// smesh/include/mesh/Transposer.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 23 2026
/*
Original's AlwaysOutTransposer: rows enter a DIM x DIM register grid, and
columns leave as the grid alternates between shifting left and shifting up.
Each DIM accepted rows switches direction. Columns of the previous matrix
leave while rows of the next matrix enter; flush rows drain the final matrix.

in_row_rdy and out_col_val are always high. Only in_row_val advances the
registers; out_col_rdy is intentionally ignored, matching Original. A pause
holds the grid and output. The caller must track useful columns: out_col_val
does not distinguish startup data, held columns, or flush data.

The first complete column appears after DIM accepted rows cross the clock
boundary. Original leaves the data registers uninitialized; this model clears
them on reset for deterministic diagnostics. Startup data is still not useful.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshTypes.hpp"

namespace smesh {

class Transposer : public Component {
  DECLARE_COMPONENT(Transposer);

 public:
  Transposer(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit,           in_row_val);
  Output(bit,          in_row_rdy);
  Input(MeshInputRow,   in_row_bits);
  Output(bit,          out_col_val);
  Input(bit,           out_col_rdy); // Present for interface compatibility; does not stall this block.
  Output(MeshInputRow,  out_col_bits);

  void updateOutput();
  void updateRegs();
  void reset() override;

 private:
  // One register per matrix element, grouped into physical grid rows.
  OutputArray(MeshInputRow,   regs_Q_, kDim);
  RegisterArray(MeshInputRow, regs_D_, kDim);
  Output(bit,   move_up_Q_); // 0: shift left; 1: shift up.
  Register(bit, move_up_D_);
  Output(u8,    count_Q_);   // Accepted rows in this direction: 0 through DIM-1.
  Register(u8,  count_D_);
};

} // namespace smesh
