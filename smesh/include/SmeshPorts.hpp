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
#include "SmeshTypes.hpp"

#include <array>
#include <cstdint>

namespace smesh {

// ********** DRIVER / SHELL INTERFACE **********
// **********************************************
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
// *********************************************
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
// ***********************************************
// interface between RS and Ld/St/Ex controllers
struct SmeshIssue {
  SmeshCmd cmd{};
  bit rs_tag_valid = true; // for completed commands, some helper commands like TransposePreloadUnroller set this false
  SmeshRsTag rs_tag = 0;
};

// ********** LOAD CONTROLLER / DMA INTERFACE **********
// *****************************************************
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
using DmaReadData = std::array<std::uint8_t, kDim * sizeof(Acc)>; // DMA reader's data is set to max possible widht (dim*accum_width)
// temp glue helper converts uint64_t to reader's byte-array payload
inline DmaReadData packDmaReadData(std::uint64_t value) {
  DmaReadData data{};
  for (std::size_t i = 0; i < data.size() && i < sizeof(value); ++i) {
    data[i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu);
  }
  return data;
}
// temp glue helper takes low bytes out of reader's byte-array payload
inline std::uint64_t low64DmaReadData(const DmaReadData& data) {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < data.size() && i < sizeof(value); ++i) {
    value |= static_cast<std::uint64_t>(data[i]) << (8 * i);
  }
  return value;
}

struct DmaReadResp {
  DmaReadData data{};
  SmeshLocalAddr laddr{};
  u8 mask = 0;
  bit has_acc_bitwidth = false;
  u32 scale = 0;
  u16 repeats = 0;
  u16 len = 0;
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
// ******************************************************
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
  u16 len = 0; // number of row elements StCtrl is writing to mem (not bytes)
  u16 block = 0;
  u16 cmd_id = 0;
  u32 status = 0;
  bit pool_en = false;
  bit store_en = true;
};
// store-side response back to StCtrl when one DmaWriteReq is accepted by the store path
struct DmaWriteResp {
  u16 cmd_id = 0;
};
// ifc to scratchpad memory read port
struct SpadReadReq {
  SmeshLocalAddr laddr{};
  u16 len = 0; // number of row elements being read from spad (not bytes)
  u16 cmd_id = 0;
  bit from_dma = true;
};
// ifc from scratchpad memory to read pipes
struct SpadReadResp {
  u64 data = 0;
  SmeshLocalAddr laddr{};
  u8 mask = 0;
  u16 len = 0; // number of row elements being read from spad (not bytes)
  u16 cmd_id = 0;
  bit from_dma = true;
};
// interface to accumulator memory read port
struct AccumReadReq {
  SmeshLocalAddr laddr{};
  u16 len = 0; // number of row elements being read from accum (not bytes)
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
  u8  mask = 0;
  u16 len = 0; // number of row elements being read from accum (not bytes)
  u8  act = 0;
  u32 scale = 0;
  bit full = false;
  u16 cmd_id = 0;
  bit from_dma = true;
};
// command metadata entering accumulator normalization
struct AccNormCmd {
  u16 len = 0; // number of row elements being read from accum (not bytes)
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
// maximum-width store data payload; len_bytes says how many bytes are meaningful
using StWriterData = std::array<std::uint8_t, kDim * sizeof(Acc)>;
// final store request after StIssueCtrl has paired metadata and data
struct StWriterReq {
  DmaWriteReq issue{};
  StWriterData data{};
  u16 len_bytes = 0; // number of bytes being written to mem (not row elements)
  bit data_is_all_zeros = false;
  bit data_is_full_width = false;
};

// ********** EXECUTE CONTROLLER / MESH INTERFACE **********
// *********************************************************
struct ExCtrlMeshPeControl {
  std::uint32_t dataflow = 0;
  bit propagate = 0;
  std::uint32_t shift = 0;
};

struct ExCtrlMeshTag {
  bit rs_tag_valid = 0;
  SmeshRsTag rs_tag = 0;
  SmeshLocalAddr addr{};
  std::uint32_t rows = 0;
  std::uint32_t cols = 0;
};

struct ExCtrlMeshReq {
  ExCtrlMeshPeControl pe_control{};
  bit a_transpose = 0;
  bit bd_transpose = 0;
  std::uint32_t total_rows = 0;
  ExCtrlMeshTag tag{};
  bit flush = 0;
};

// mesh input row payload from ExCtrl into Mesher
struct ExCtrlMeshInput {
  u64 data = 0;
  bit valid = 0;
};

struct MesherResp {
  u64 data = 0;
  std::uint32_t total_rows = 0;
  ExCtrlMeshTag tag{};
  bit last = 0;
};

using MesherTag = ExCtrlMeshTag;

} // namespace smesh
