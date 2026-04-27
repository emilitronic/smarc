// **********************************************************************
// smesh/src/SmeshDevice.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Apr 26 2026

#include "SmeshDevice.hpp"

#include <stdexcept>
#include <string>

namespace smesh {

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

} // namespace

void SmeshState::reset() {
  for (auto& row : spad) {
    row.fill(0);
  }
  for (auto& row : accumulator) {
    row.fill(0);
  }
  for (auto& row : pe_state) {
    row.fill(0);
  }

  preload_sp_row = 0;
  output_acc_row = 0;
  preload_shape = {};
  output_shape = {};
}

void SmeshDevice::reset() {
  state_.reset();
}

// mvin: move a matrix from host memory into the scratchpad
void SmeshDevice::mvin(SmeshMemory& mem,
                       std::uint64_t dram_addr,
                       std::uint32_t spad_row,
                       MatrixShape shape,
                       std::uint32_t stride_bytes) {
  checkSpadRange(spad_row, shape);
  require(stride_bytes >= shape.cols * sizeof(Elem), "mvin stride is too small");

  for (std::size_t r = 0; r < shape.rows; ++r) {
    for (std::size_t c = 0; c < shape.cols; ++c) {
      state_.spad.at(spad_row + r).at(c) =
          mem.readElem(dram_addr + r * stride_bytes + c * sizeof(Elem));
    }
  }
}

// preload: move a matrix from the scratchpad into the PE state and set up for compute
void SmeshDevice::preload(std::uint32_t b_spad_row,
                          std::uint32_t c_acc_row,
                          MatrixShape b_shape,
                          MatrixShape c_shape) {
  checkSpadRange(b_spad_row, b_shape);
  checkAccRange(c_acc_row, c_shape);
  checkDimShape(b_shape);
  checkDimShape(c_shape);

  state_.preload_sp_row = b_spad_row;
  state_.output_acc_row = c_acc_row;
  state_.preload_shape = b_shape;
  state_.output_shape = c_shape;

  for (auto& row : state_.pe_state) {
    row.fill(0);
  }
  // preload B into the PE state
  for (std::size_t r = 0; r < b_shape.rows; ++r) {
    for (std::size_t c = 0; c < b_shape.cols; ++c) {
      state_.pe_state.at(r).at(c) = state_.spad.at(b_spad_row + r).at(c);
    }
  }
}

// run A against what is currently in the PE state, and write the result into the accumulator
void SmeshDevice::computePreloaded(std::uint32_t a_spad_row, MatrixShape a_shape) {
  checkSpadRange(a_spad_row, a_shape);
  checkDimShape(a_shape);

  const MatrixShape b_shape = state_.preload_shape;
  const MatrixShape c_shape = state_.output_shape;
  checkDimShape(b_shape);
  checkDimShape(c_shape);

  require(a_shape.cols == b_shape.rows, "compute shape mismatch");
  require(c_shape.rows == a_shape.rows, "compute output row mismatch");
  require(c_shape.cols == b_shape.cols, "compute output col mismatch");

  for (std::size_t r = 0; r < c_shape.rows; ++r) {
    for (std::size_t c = 0; c < c_shape.cols; ++c) {
      Acc sum = 0;
      for (std::size_t k = 0; k < a_shape.cols; ++k) {
        sum += static_cast<Acc>(state_.spad.at(a_spad_row + r).at(k)) *
               state_.pe_state.at(k).at(c);
      }
      state_.accumulator.at(state_.output_acc_row + r).at(c) = sum;
    }
  }
}

// mvout: move a matrix from the accumulator into host memory
void SmeshDevice::mvout(SmeshMemory& mem,
                        std::uint64_t dram_addr,
                        std::uint32_t acc_row,
                        MatrixShape shape,
                        std::uint32_t stride_bytes) const {
  checkAccRange(acc_row, shape);
  require(stride_bytes >= shape.cols * sizeof(Acc), "mvout stride is too small");

  for (std::size_t r = 0; r < shape.rows; ++r) {
    for (std::size_t c = 0; c < shape.cols; ++c) {
      mem.writeAcc(dram_addr + r * stride_bytes + c * sizeof(Acc),
                   state_.accumulator.at(acc_row + r).at(c));
    }
  }
}

// Can matrix tile of size rows x cols fit in SP starting at row?
void SmeshDevice::checkSpadRange(std::uint32_t row, MatrixShape shape) {
  checkDimShape(shape);
  require(row + shape.rows <= kScratchpadRows, "scratchpad row range out of bounds");
}

void SmeshDevice::checkAccRange(std::uint32_t row, MatrixShape shape) {
  checkDimShape(shape);
  require(row + shape.rows <= kAccumulatorRows, "accumulator row range out of bounds");
}

void SmeshDevice::checkDimShape(MatrixShape shape) {
  require(shape.rows <= kDim, "matrix rows exceed tile dim");
  require(shape.cols <= kDim, "matrix cols exceed tile dim");
}

} // namespace smesh
