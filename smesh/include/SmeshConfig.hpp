// **********************************************************************
// smesh/include/SmeshConfig.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jun 24 2026
/*
Central hardware configuration constants for the current smesh model.

This intentionally starts with the settings the simulator uses today, plus the
small reservation-station capacities planned for M4v0. Keep larger settings out 
until there is code which consumes them.
*/
#pragma once

#include <cstddef>

namespace smesh {

struct SmeshConfig {
  std::size_t dim = 4;

  std::size_t sp_banks       =  4;
  std::size_t sp_bank_rows   =  4;
  std::size_t spad_read_delay = 4; // cycles from an accepted SPAD read to its response
  bool sp_singleported = true; // same-bank writes take priority over reads

  std::size_t acc_banks     =  2;
  std::size_t acc_bank_rows =  8;
  bool acc_singleported = true; // same-bank writes take priority over reads

  std::size_t load_states   =  3;
  bool has_first_layer_optimizations = false;
  std::size_t max_in_flight_mem_reqs = 16;

  std::size_t elem_bits     =  8;
  std::size_t acc_bits      = 32;
  std::size_t dma_max_bytes = 64;

  bool has_max_pool = true; // StoreController includes max-pooling geometry
  std::size_t store_cmd_tracker_entries = 2; // concurrent StoreController commands

  std::size_t rs_load_entries    = 2; // typ: 8
  std::size_t rs_execute_entries = 8; // typ: 4 -- experiment: testing mul_pre-through-RS timing
  std::size_t rs_store_entries   = 2; // typ: 16
  std::size_t ex_queue_length    = 8; // ExCtrl cmd q len
  std::size_t max_simultaneous_matmuls = 5; // set counter size in Mesher logic

  bool ex_read_from_acc = true; // true: ExCtrl reads from accum when local addr says accum
  bool ex_write_to_spad = true;
  bool ex_write_to_acc  = true; // true: ExCtrl can write mesh o/p to accum
  std::size_t aligned_to = 1; // ExCtrl uses this to  size/expand write masks in writeback logic. 1=byte, 2=halfword, 4=word, 8=doubleword
};

constexpr SmeshConfig kDefaultConfig{};

} // namespace smesh
