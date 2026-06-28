// **********************************************************************
// smesh/include/SmeshConfig.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jun 24 2026
/*
Central configuration constants for the current smesh model.

This intentionally starts with the settings the simulator uses today, plus the
small reservation-station capacities planned for M4v0. Keep larger Gemmini-like
settings out until there is code which consumes them.
*/
#pragma once

#include <cstddef>

namespace smesh {

struct SmeshConfig {
  std::size_t dim = 4;

  std::size_t sp_banks = 4;
  std::size_t sp_bank_rows = 4;

  std::size_t acc_banks = 2;
  std::size_t acc_bank_rows = 8;

  std::size_t load_states = 3;

  std::size_t elem_bits = 8;
  std::size_t acc_bits = 32;
  std::size_t dma_max_bytes = 64;

  std::size_t rs_load_entries = 1;
  std::size_t rs_execute_entries = 1;
  std::size_t rs_store_entries = 1;
};

constexpr SmeshConfig kDefaultConfig{};

} // namespace smesh
