// **********************************************************************
// smesh/src/mesh/Transposer.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 23 2026
#include "Transposer.hpp"

namespace smesh {

TraceKey(transposer_);

Transposer::Transposer(std::string /*name*/, IMPL_CTOR) {
  static_assert((kDim & (kDim - 1)) == 0, "Transposer DIM must be a power of two");
  for (std::size_t row = 0; row < kDim; ++row) regs_Q_[row] <= regs_D_[row];
  move_up_Q_ <= move_up_D_;
  count_Q_ <= count_D_;

  UPDATE(updateOutput).reads(regs_Q_, move_up_Q_)
                      .writes(in_row_rdy, out_col_val, out_col_bits);
  UPDATE(updateRegs).reads(in_row_val, in_row_bits, regs_Q_, move_up_Q_, count_Q_)
                    .writes(regs_D_, move_up_D_, count_D_);
}

// Present the grid's left edge or top edge as the column going toward the mesh.
void Transposer::updateOutput() {
  in_row_rdy = 1;
  out_col_val = 1;
  MeshInputRow column{};
  for (std::size_t element = 0; element < kDim; ++element) {
    column[element] = move_up_Q_ == 1 ? (*regs_Q_[0])[element] : (*regs_Q_[element])[0];
  }
  out_col_bits = column;
}

// Shift every grid register on an accepted row; all sources are current Q values.
void Transposer::updateRegs() {
  if (in_row_val == 0) return;

  const bool move_up = move_up_Q_ == 1;
  const auto input = *in_row_bits;
  for (std::size_t row = 0; row < kDim; ++row) {
    MeshInputRow next{};
    for (std::size_t col = 0; col < kDim; ++col) {
      if (move_up) {
        next[col] = row == kDim - 1 ? input[col] : (*regs_Q_[row + 1])[col];
      } else {
        next[col] = col == kDim - 1 ? input[row] : (*regs_Q_[row])[col + 1];
      }
    }
    regs_D_[row] = next;
  }

  // Switch the input/output edges after one complete matrix's rows have entered.
  const auto count = static_cast<unsigned>(*count_Q_);
  const bool last_row = count == kDim - 1;
  count_D_ = u8(last_row ? 0 : count + 1);
  if (last_row) move_up_D_ = bit(!move_up);
  trace(transposer_, "advance dir=%s count=%u next_dir=%s next_count=%u\n",
        move_up ? "up" : "left", count,
        (move_up != last_row) ? "up" : "left", last_row ? 0 : count + 1);
}

void Transposer::reset() {
  for (std::size_t row = 0; row < kDim; ++row) {
    regs_Q_[row].reset(MeshInputRow{});
    regs_D_[row].reset(MeshInputRow{});
  }
  move_up_Q_.reset(0);
  move_up_D_.reset(0);
  count_Q_.reset(0);
  count_D_.reset(0);
  in_row_rdy.reset(1);
  out_col_val.reset(1);
  out_col_bits.reset(MeshInputRow{});
}

} // namespace smesh
