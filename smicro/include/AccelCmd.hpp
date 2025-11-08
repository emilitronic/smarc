// **********************************************************************
// smicro/include/AccelCmd.hpp
// **********************************************************************
// S Magierowski Aug 16 2025

#pragma once
#include <cstdint>

// Command descriptor for NN accelerator (matmul + optional ReLU)
struct AccelCmd {
  u64 a_base = 0;  // base address of A matrix
  u64 b_base = 0;  // base address of B matrix
  u64 c_base = 0;  // base address of C (output)
  u32 m = 0, n = 0, k = 0; // dimensions
  bool relu = true;        // apply ReLU to outputs
};

