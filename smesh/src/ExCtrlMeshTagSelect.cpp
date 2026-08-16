// **********************************************************************
// smesh/src/ExCtrlMeshTagSelect.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 16 2026

#include "ExCtrlMeshTagSelect.hpp"

namespace smesh {

ExCtrlMeshTagSelect::ExCtrlMeshTagSelect(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(head_val,
             head_bits,
             preload_cmd_place,
             performing_single_mul,
             c_address_rs2)
      .writes(mesh_rs_tag_valid, mesh_rs_tag);
}

void ExCtrlMeshTagSelect::update() {
  const auto place = static_cast<std::size_t>(*preload_cmd_place);

  mesh_rs_tag_valid = 0;
  mesh_rs_tag = 0;

  if (place >= kExCtrlCmdWindow || head_val[place] == 0) {
    return;
  }

  const auto issue = *head_bits[place];

  mesh_rs_tag = issue.rs_tag;
  mesh_rs_tag_valid = bit(performing_single_mul == 0 &&
                          !(*c_address_rs2).is_garbage() &&
                          issue.rs_tag_valid != 0);
}

} // namespace smesh
