// **********************************************************************
// smesh/src/StIssueMux.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 14 2026
/*
Store-path final issue payload mux implementation.
*/

#include "StIssueMux.hpp"

#include "SmeshTypes.hpp"

namespace smesh {

namespace {

enum : std::uint8_t {
  kDataSourceZero = 0,
  kDataSourceSpad = 1,
  kDataSourceAcc  = 2,
};

enum : std::uint8_t {
  kFinalDataZero         = 0,
  kFinalDataNormalWidth  = 1,
  kFinalDataFullAccWidth = 2,
};

StWriterData packStoreData(std::uint64_t value) {
  StWriterData data{};
  for (std::size_t i = 0; i < data.size() && i < sizeof(value); ++i) {
    data[i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu);
  }
  return data;
}

StWriterData packStoreData(const MeshInputRow& row) {
  StWriterData data{};
  for (std::size_t i = 0; i < kDim && i < data.size(); ++i) {
    data[i] = static_cast<std::uint8_t>(row[i]);
  }
  return data;
}

StWriterData packStoreData(const MeshAccumRow& row) {
  StWriterData data{};
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    const auto value = static_cast<std::uint32_t>(row[lane]);
    for (std::size_t byte = 0; byte < sizeof(Acc); ++byte) {
      data[lane * sizeof(Acc) + byte] = static_cast<std::uint8_t>((value >> (8 * byte)) & 0xffu);
    }
  }
  return data;
}

} // namespace

StIssueMux::StIssueMux(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(issue_bits,
                       spad_data_bits,
                       acc_data_bits,
                       data_source_sel,
                       final_data_sel,
                       write_data_is_all_zeros,
                       write_data_is_full_width)
                .writes(writer_req_bits);
}

void StIssueMux::update() {
  // payloads come into mux
  const auto issue = *issue_bits;
  const auto spad  = *spad_data_bits[issue.laddr.sp_bank()];
  const auto acc   = *acc_data_bits;

  StWriterReq req{}; // blank o/p request and default settings (below)
  req.issue              = issue;
  req.data_is_all_zeros  = write_data_is_all_zeros;
  req.data_is_full_width = write_data_is_full_width;
  // convert len from row elements to bytes depending on whether the write is full-width (Acc) or normal-width (Elem)
  req.len_bytes = u16(static_cast<std::uint16_t>( issue.len * (req.data_is_full_width ? sizeof(Acc) : sizeof(Elem))));

  StWriterData selected_data{};
  switch (static_cast<std::uint8_t>(data_source_sel)) {
    case kDataSourceSpad:
      selected_data = packStoreData(spad.data);
      break;
    case kDataSourceAcc:
      selected_data = static_cast<std::uint8_t>(final_data_sel) == kFinalDataFullAccWidth
          ? packStoreData(acc.full_data)
          : packStoreData(acc.data);
      break;
    case kDataSourceZero:
    default:
      selected_data = StWriterData{};
      break;
  }

  req.data = selected_data;

  writer_req_bits = req;
}

} // namespace smesh
