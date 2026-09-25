// **********************************************************************
// smesh/src/controllers/ld/LdCtrlCmdDec.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include "LdCtrlCmdDec.hpp"

#include "SmeshCommand.hpp"

namespace smesh {

LdCtrlCmdDec::LdCtrlCmdDec(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(head_val, head_bits)
                .writes(do_config, do_load, load_state_id, config_state_id,
                        state_id, vaddr, localaddr, rows)
                .writes(cols)
                .writes(config_stride, config_scale, config_shrink,
                        config_block_stride, config_pixel_repeats);
}

void LdCtrlCmdDec::update() {
  do_config = 0;
  do_load = 0;
  load_state_id = 0;
  config_state_id = 0;
  state_id = 0;
  vaddr = 0;
  localaddr = SmeshLocalAddr{};
  rows = 0;
  cols = 0;
  config_stride = 0;
  config_scale = 0;
  config_shrink = 0;
  config_block_stride = 0;
  config_pixel_repeats = 0;

  if (head_val == 0) {
    return;
  }

  const auto issue = *head_bits;
  const auto funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(issue.cmd.funct));
  const auto rs1 = static_cast<std::uint64_t>(issue.cmd.rs1);
  const auto rs2 = static_cast<std::uint64_t>(issue.cmd.rs2);

  if (funct == SmeshFunct::Config) {
    do_config = 1;
    const auto slot = static_cast<std::uint8_t>(unpackConfigStateId(rs1));
    config_state_id = slot;
    state_id = slot;
    config_stride = rs2;
    config_scale = static_cast<std::uint32_t>(rs1 >> 32);
    config_shrink = bit(((rs1 >> 2) & 1u) == 1u);
    config_block_stride = static_cast<std::uint16_t>(unpackConfigLoadBlockStride(rs1));
    config_pixel_repeats = static_cast<std::uint8_t>((rs1 >> 8) & 0xffu);
    return;
  }

  // The RS is responsible for routing only load commands to this queue.
  do_load = 1;
  const auto slot = funct == SmeshFunct::Mvin2 ? 1 : funct == SmeshFunct::Mvin3 ? 2 : 0;
  load_state_id = slot;
  state_id = slot;
  vaddr = rs1;
  const auto source = unpackLocal(rs2);
  localaddr = makeLocalAddr(source.row);
  rows = static_cast<std::uint32_t>(source.shape.rows);
  cols = static_cast<std::uint32_t>(source.shape.cols);
}

} // namespace smesh
