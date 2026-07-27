// **********************************************************************
// smesh/src/ExCtrlCompletion.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 27 2026

#include "ExCtrlCompletion.hpp"

namespace smesh {

ExCtrlCompletion::ExCtrlCompletion(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(pending_completed_valid);
}

void ExCtrlCompletion::update() {
  pending_completed_valid =
      bit(pending_completed_valid_[0] || pending_completed_valid_[1]);
}

void ExCtrlCompletion::reset() {
  pending_completed_valid_[0] = false;
  pending_completed_valid_[1] = false;
  pending_completed_bits_[0] = 0;
  pending_completed_bits_[1] = 0;

  pending_completed_valid.reset(0);
}

} // namespace smesh
