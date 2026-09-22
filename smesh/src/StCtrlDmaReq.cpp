// **********************************************************************
// smesh/src/StCtrlDmaReq.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026

#include "StCtrlDmaReq.hpp"

namespace smesh {

namespace {

constexpr std::uint32_t kNormSum       = 1;
constexpr std::uint32_t kNormMean      = 2;
constexpr std::uint32_t kNormVariance  = 3;
constexpr std::uint32_t kNormInvStddev = 4;
constexpr std::uint32_t kNormMax       = 5;
constexpr std::uint32_t kNormSumExp    = 6;
constexpr std::uint32_t kNormInvSumExp = 7;

std::uint32_t nonResetNormCmd(std::uint32_t cmd) {
  switch (cmd) {
    case kNormMean:      return kNormSum;
    case kNormMax:       return kNormMax;
    case kNormInvStddev: return kNormVariance;
    case kNormInvSumExp: return kNormSumExp;
    default:             return cmd;
  }
}

SmeshLocalAddr withNormCmd(SmeshLocalAddr addr, std::uint32_t cmd) {
  constexpr std::uint32_t kNormMask = 0x7u << kLocalAddrNormShift;
  addr.raw = (addr.raw & ~kNormMask) |
             ((cmd & 0x7u) << kLocalAddrNormShift);
  return addr;
}

} // namespace

StCtrlDmaReq::StCtrlDmaReq(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(dst_is_spad,
             cols,
             blocks,
             mstatus,
             pooling_is_enabled,
             mvout_1d_enabled,
             current_vaddr,
             current_localaddr)
      .reads(current_dst_spad_addr,
             pool_row_addr,
             pool_vaddr,
             activation,
             acc_scale,
             igelu_qb,
             igelu_qc,
             iexp_qln2)
      .reads(iexp_qln2_inv,
             norm_stats_id,
             block_counter,
             wrow_counter,
             wcol_counter,
             pool_size,
             cmd_id)
      .writes(req_bits);
}

void StCtrlDmaReq::update() {
  const bool to_spad = dst_is_spad == 1;
  const bool pooling = pooling_is_enabled == 1;
  const bool moveout_1d = mvout_1d_enabled == 1;
  const auto block = static_cast<std::uint32_t>(*block_counter);
  const auto block_count = static_cast<std::uint32_t>(*blocks);
  const auto column_count = static_cast<std::uint32_t>(*cols);
  const auto window_size = static_cast<std::uint32_t>(*pool_size);
  const auto window_row = static_cast<std::uint32_t>(*wrow_counter);
  const auto window_col = static_cast<std::uint32_t>(*wcol_counter);
  const bool has_blocks = block_count != 0;
  const bool last_block = has_blocks && block == block_count - 1;

  DmaWriteReq req{};
  req.vaddr = to_spad
                  ? *current_dst_spad_addr
                  : ((pooling || moveout_1d) ? *pool_vaddr : *current_vaddr);
  req.dest = static_cast<u16>(to_spad ? 1u : 0u);

  const auto current_addr = *current_localaddr;
  req.laddr = pooling ? *pool_row_addr : current_addr;
  const auto norm_cmd = last_block
                            ? current_addr.norm_cmd()
                            : nonResetNormCmd(current_addr.norm_cmd());
  req.laddr = withNormCmd(req.laddr, norm_cmd);

  req.acc_act = *activation;
  req.acc_scale = *acc_scale;
  req.acc_igelu_qb = *igelu_qb;
  req.acc_igelu_qc = *igelu_qc;
  req.acc_iexp_qln2 = *iexp_qln2;
  req.acc_iexp_qln2_inv = *iexp_qln2_inv;
  req.acc_norm_stats_id = *norm_stats_id;

  // A zero-size command is invalid, but keep C++ arithmetic defined while
  // command legality is still owned by the future handshake block.
  req.len = static_cast<u16>(
      !has_blocks || column_count == 0
          ? 0
          : (last_block ? ((column_count - 1) % kDim) + 1 : kDim));
  req.block = static_cast<u16>(block);
  req.cmd_id = *cmd_id;
  req.status = *mstatus;
  req.pool_en = bit(pooling && (window_row != 0 || window_col != 0));
  req.store_en = bit(pooling
                         ? window_size != 0 &&
                               window_row == window_size - 1 &&
                               window_col == window_size - 1
                         : last_block);

  req_bits = req;
}

void StCtrlDmaReq::reset() {
  req_bits.reset(DmaWriteReq{});
}

} // namespace smesh
