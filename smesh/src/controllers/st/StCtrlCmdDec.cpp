// **********************************************************************
// smesh/src/controllers/st/StCtrlCmdDec.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
/*
Store-controller head-command decoder implementation.
*/

#include "StCtrlCmdDec.hpp"

#include "SmeshCommand.hpp"

namespace smesh {

namespace {

constexpr std::uint32_t kConfigStoreType = 2;
constexpr std::uint32_t kConfigNormType  = 3;

std::uint32_t bits32(std::uint64_t value, unsigned shift) {
  return static_cast<std::uint32_t>((value >> shift) & 0xffffffffull);
}

std::uint8_t bits8(std::uint64_t value, unsigned shift) {
  return static_cast<std::uint8_t>((value >> shift) & 0xffu);
}

std::uint8_t bits2(std::uint64_t value, unsigned shift) {
  return static_cast<std::uint8_t>((value >> shift) & 0x3u);
}

} // namespace

StCtrlCmdDec::StCtrlCmdDec(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(head_val, head_bits)
                .writes(do_config,
                        do_config_norm,
                        do_store,
                        dst_is_spad,
                        vaddr,
                        dst_spad_addr,
                        dst_spad_stride,
                        localaddr)
                .writes(rows,
                        cols,
                        blocks,
                        config_cmd_type,
                        config_stride,
                        config_activation,
                        config_acc_scale,
                        config_pool_stride)
                .writes(config_pool_size,
                        config_pool_out_dim,
                        config_porows,
                        config_pocols,
                        config_orows,
                        config_ocols,
                        config_upad,
                        config_lpad)
                .writes(config_stats_id,
                        config_activation_msb,
                        config_set_stats_id_only,
                        config_iexp_q_const_type,
                        config_iexp_q_const,
                        config_igelu_qb,
                        config_igelu_qc,
                        mstatus);
}

void StCtrlCmdDec::update() {
  do_config       = 0;
  do_config_norm  = 0;
  do_store        = 0;
  dst_is_spad     = 0;
  vaddr           = 0;
  dst_spad_addr   = SmeshLocalAddr{};
  dst_spad_stride = 0;
  localaddr       = SmeshLocalAddr{};
  rows            = 0;
  cols            = 0;
  blocks          = 0;
  config_cmd_type = 0;
  config_stride   = 0;
  config_activation = 0;
  config_acc_scale  = 0;
  config_pool_stride = 0;
  config_pool_size   = 0;
  config_pool_out_dim = 0;
  config_porows = 0;
  config_pocols = 0;
  config_orows  = 0;
  config_ocols  = 0;
  config_upad   = 0;
  config_lpad   = 0;
  config_stats_id = 0;
  config_activation_msb = 0;
  config_set_stats_id_only = 0;
  config_iexp_q_const_type = 0;
  config_iexp_q_const = 0;
  config_igelu_qb = 0;
  config_igelu_qc = 0;
  mstatus = 0;

  if (head_val == 0) {
    return;
  }

  const auto issue = *head_bits;
  const auto funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(issue.cmd.funct));
  const auto rs1 = static_cast<std::uint64_t>(issue.cmd.rs1);
  const auto rs2 = static_cast<std::uint64_t>(issue.cmd.rs2);
  const auto cmd_type = static_cast<std::uint32_t>(rs1 & 0x3u);

  // TODO: add command status to SmeshCmd before forwarding mstatus.
  mstatus = 0;
  config_cmd_type = static_cast<std::uint8_t>(cmd_type);

  const bool is_config = funct == SmeshFunct::Config;
  const bool is_config_store = is_config && cmd_type == kConfigStoreType;
  const bool is_config_norm = is_config && cmd_type == kConfigNormType;
  const bool is_store_cmd = funct == SmeshFunct::Mvout || funct == SmeshFunct::StoreSpad;

  do_config      = bit(is_config_store);
  do_config_norm = bit(is_config_norm);
  do_store       = bit(is_store_cmd);
  dst_is_spad    = bit(funct == SmeshFunct::StoreSpad);

  // STORE and STORE_SPAD share the source matrix layout in rs2.
  const auto source = unpackLocal(rs2);
  localaddr = makeLocalAddr(source.row);
  rows = static_cast<std::uint32_t>(source.shape.rows);
  cols = static_cast<std::uint32_t>(source.shape.cols);
  blocks = static_cast<std::uint32_t>((source.shape.cols + kDim - 1) / kDim);
  vaddr = rs1;

  if (funct == SmeshFunct::StoreSpad) {
    dst_spad_addr = makeLocalAddr(static_cast<std::uint32_t>(rs1));
    dst_spad_stride = bits32(rs1, 32);
  }

  // CONFIG_STORE uses ConfigMvoutRs1/ConfigMvoutRs2's fixed model widths.
  if (is_config_store) {
    config_activation   = bits2(rs1, 2);
    config_pool_stride  = bits2(rs1, 4);
    config_pool_size    = bits2(rs1, 6);
    config_upad         = bits2(rs1, 8);
    config_lpad         = bits2(rs1, 10);
    config_pool_out_dim = bits8(rs1, 24);
    config_porows       = bits8(rs1, 32);
    config_pocols       = bits8(rs1, 40);
    config_orows        = bits8(rs1, 48);
    config_ocols        = bits8(rs1, 56);
    config_stride       = bits32(rs2, 0);
    config_acc_scale    = bits32(rs2, 32);
  }

  // CONFIG_NORM uses ConfigNormRs1/ConfigNormRs2's fixed 32-bit fields.
  if (is_config_norm) {
    config_stats_id          = bits8(rs1, 8);
    config_activation_msb    = bit(((rs1 >> 16) & 0x1u) != 0);
    config_set_stats_id_only = bit(((rs1 >> 17) & 0x1u) != 0);
    config_iexp_q_const_type = bit(((rs1 >> 18) & 0x1u) != 0);
    config_iexp_q_const       = bits32(rs1, 32);
    config_igelu_qb           = bits32(rs2, 0);
    config_igelu_qc           = bits32(rs2, 32);
  }
}

} // namespace smesh
