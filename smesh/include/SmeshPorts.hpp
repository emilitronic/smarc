// **********************************************************************
// smesh/include/SmeshPorts.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski May 10 2026
/*
A collection of ports between smesh components.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshLocalAddr.hpp"

#include <cstdint>

namespace smesh {

// ********** DRIVER / SHELL INTERFACE **********
// interface between SmeshCommandDriver and SmeshShell
struct SmeshCmd {
  u32 funct = 0;
  u64 rs1 = 0;
  u64 rs2 = 0;
};

struct SmeshResp {
  u8 status = 0;
  u64 value = 0;
};

// ********** COMMAND QUEUE INTERFACE **********
// command wrapper after command ingress, before RS allocation
using SmeshRsTag = std::uint16_t;

struct SmeshQueuedCmd {
  SmeshCmd cmd{};
  SmeshRsTag rs_tag   = 0;
  bit rs_tag_valid    = false;
  bit from_mmul_loop  = false;
  bit from_conv_loop  = false;
};

// ********** RS / CONTROLLER INTERFACE **********
// interface between RS and Ld/St/Ex controllers

struct SmeshIssue {
  SmeshCmd cmd{};
  SmeshRsTag rs_tag = 0;
};

// ********** LOAD CONTROLLER / DMA INTERFACE **********
// interface between LdCtrl and memory controller
struct DmaReadReq {
  u64 vaddr = 0;
  SmeshLocalAddr laddr{};
  u16 cols = 0;
  u16 repeats = 0;
  u32 scale = 0;
  bit has_acc_bitwidth = false;
  bit all_zeros = false;
  u16 block_stride = 0;
  u8 pixel_repeats = 1;
  u16 cmd_id = 0;
};
// interface between memory controller and LdCtrl (via other components)
struct DmaReadResp {
  u64 data = 0;
  SmeshLocalAddr laddr{};
  u8 mask = 0;
  u16 bytes_read = 0;
  u8 pixel_repeats = 1;
  u16 cmd_id = 0;
  bit last = false;
};
// let LdCtrl know the last write into local mem is done
struct DmaReadCompletion {
  u16 bytes_read = 0;
  u16 cmd_id = 0;
};

// ********** STORE CONTROLLER / DMA INTERFACE **********
// interface between StCtrl and the store-side write dispatch path
struct DmaWriteReq {
  u64 vaddr = 0;
  SmeshLocalAddr laddr{};
  u16 dest = 0;
  u8 acc_act = 0;
  u32 acc_scale = 0;
  u32 acc_igelu_qb = 0;
  u32 acc_igelu_qc = 0;
  u32 acc_iexp_qln2 = 0;
  u32 acc_iexp_qln2_inv = 0;
  u16 acc_norm_stats_id = 0;
  u16 len = 0;
  u16 block = 0;
  u16 cmd_id = 0;
  u32 status = 0;
  bit pool_en = false;
  bit store_en = true;
};

// ifc to scratchpad memory read port
struct SpadReadReq {
  SmeshLocalAddr laddr{};
  u16 len = 0;
  u16 cmd_id = 0;
  bit from_dma = true;
};
// ifc from scratchpad memory to read pipes
struct SpadReadResp {
  u64 data = 0;
  SmeshLocalAddr laddr{};
  u8 mask = 0;
  u16 len = 0;
  u16 cmd_id = 0;
  bit from_dma = true;
};

// interface to accumulator memory read port
struct AccumReadReq {
  SmeshLocalAddr laddr{};
  u16 len = 0;
  u8 act = 0;
  u32 scale = 0;
  u32 igelu_qb = 0;
  u32 igelu_qc = 0;
  u32 iexp_qln2 = 0;
  u32 iexp_qln2_inv = 0;
  bit full = false;
  u16 cmd_id = 0;
  bit from_dma = true;
};
// ifc from accumulator memory to read pipes to normalizer
struct AccumReadResp {
  u64 data = 0;
  SmeshLocalAddr laddr{};
  u8 mask = 0;
  u16 len = 0;
  u8 act = 0;
  u32 scale = 0;
  bit full = false;
  u16 cmd_id = 0;
  bit from_dma = true;
};

// command metadata entering accumulator normalization
struct AccNormCmd {
  u16 len = 0;
  u16 stats_id = 0;
  u8 cmd = 0;
};

// joined accumulator data + normalization metadata
struct AccNormReq {
  AccumReadResp acc_read_resp{};
  AccNormCmd cmd{};
};

// accumulator data entering the accumulator scale stage
struct AccScaleReq {
  AccNormReq norm{};
};

// accumulator data after the accumulator scale stage
struct AccScaleResp {
  u64 full_data = 0;
  u64 data = 0;
  u16 acc_bank_id = 0;
  bit from_dma = true;
};

} // namespace smesh
