// **********************************************************************
// smesh/include/SmeshDevice.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Apr 26 2026
/*
Actual functional smesh device model.
*/
#pragma once

#include "SmeshCommand.hpp" // for SmeshFunct and command encoding helpers
#include "SmeshMemory.hpp"
#include "SmeshState.hpp"
#include "SmeshTypes.hpp"

#include <cstdint>

namespace smesh {

class SmeshDevice {
 public:
  void reset();

  // executeCustom provides a generic command interface.  
  // The semantics of rs1 and rs2 depend on the command (funct).
  std::uint64_t executeCustom(SmeshMemory& mem,
                              SmeshFunct funct,
                              std::uint64_t rs1,
                              std::uint64_t rs2);

  void mvin(SmeshMemory& mem,
            std::uint64_t dram_addr,
            std::uint32_t spad_row,
            MatrixShape shape,
            std::uint32_t stride_bytes);

  void preload(std::uint32_t b_spad_row,
               std::uint32_t c_acc_row,
               MatrixShape b_shape,
               MatrixShape c_shape);

  void computePreloaded(std::uint32_t a_spad_row, MatrixShape a_shape);

  void mvout(SmeshMemory& mem,
             std::uint64_t dram_addr,
             std::uint32_t acc_row,
             MatrixShape shape,
             std::uint32_t stride_bytes) const;

  const SmeshState& state() const { return state_; }

 private:
  static void checkSpadRange(std::uint32_t row, MatrixShape shape); // starting row, and shape
  static void checkAccRange(std::uint32_t row, MatrixShape shape);
  static void checkDimShape(MatrixShape shape);

  SmeshState state_;
};

} // namespace smesh
