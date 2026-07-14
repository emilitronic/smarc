// **********************************************************************
// smesh/src/StIssueMux.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 14 2026
/*
Store-path final issue payload mux implementation.
*/

#include "StIssueMux.hpp"

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
  const auto spad  = *spad_data_bits;
  const auto acc   = *acc_data_bits;

  StWriterReq req{}; // blank o/p request and default settings (below)
  req.issue              = issue;
  req.data_is_all_zeros  = write_data_is_all_zeros;
  req.data_is_full_width = write_data_is_full_width;

  switch (static_cast<std::uint8_t>(data_source_sel)) {
    case kDataSourceSpad:
      req.data      = spad.data;
      req.full_data = spad.data;
      break;
    case kDataSourceAcc:
      req.data      = acc.data;
      req.full_data = acc.full_data;
      break;
    case kDataSourceZero:
    default:
      req.data      = 0;
      req.full_data = 0;
      break;
  }

  if (static_cast<std::uint8_t>(final_data_sel) == kFinalDataFullAccWidth) {
    req.data = req.full_data;
  }

  writer_req_bits = req;
}

} // namespace smesh
