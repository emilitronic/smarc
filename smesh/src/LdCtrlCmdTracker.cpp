// **********************************************************************
// smesh/src/LdCtrlCmdTracker.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include "LdCtrlCmdTracker.hpp"

namespace smesh {

TraceKey(ld_ctrl_tracker_);

LdCtrlCmdTracker::LdCtrlCmdTracker(std::string /*name*/, IMPL_CTOR) {
  tracker_Q_ <= tracker_D_;
  UPDATE(updateView).reads(tracker_Q_)
                    .writes(alloc_rdy, alloc_cmd_id, completed_val,
                            completed_bits, completed_cmd_id, busy);
  UPDATE(updateState)
      .reads(tracker_Q_, alloc_val, alloc_rdy, alloc_bytes_to_read,
             alloc_rs_tag, alloc_cmd_id, returned_val, returned_cmd_id)
      .reads(returned_bytes_read, completed_val, completed_rdy, completed_cmd_id)
      .writes(tracker_D_);
}

void LdCtrlCmdTracker::updateView() {
  const auto q        = *tracker_Q_;
  bool found_free     = false;
  bool found_complete = false;
  bool any_valid      = false;
  std::size_t free_id = 0;
  std::size_t complete_id = 0;

  for (std::size_t i = 0; i < kLoadCmdTrackerEntries; ++i) { // 5 entries for 16 in-flight mem reqs and 4x4 systolic
    const auto& entry = q.entries[i];
    any_valid |= entry.valid == 1;
    if (!found_free && entry.valid == 0) {
      found_free = true;
      free_id = i;
    }
    if (!found_complete && entry.valid == 1 && entry.bytes_left == 0) {
      found_complete = true;
      complete_id = i;
    }
  }

  alloc_rdy = bit(found_free);
  alloc_cmd_id = static_cast<std::uint16_t>(free_id);
  completed_val = bit(found_complete);
  completed_cmd_id = static_cast<std::uint16_t>(complete_id);
  completed_bits = found_complete ? q.entries[complete_id].rs_tag : 0;
  busy = bit(any_valid);
}

void LdCtrlCmdTracker::updateState() {
  const auto q = *tracker_Q_;
  auto next = q;

  if (completed_val == 1 && completed_rdy == 1) {
    const auto id = static_cast<std::size_t>(*completed_cmd_id);
    next.entries[id] = LdCtrlCmdTrackerEntry{};
    trace(ld_ctrl_tracker_, "complete id=%u tag=%u\n",
          static_cast<unsigned>(id), static_cast<unsigned>(q.entries[id].rs_tag));
  }

  if (returned_val == 1) {
    const auto id = static_cast<std::size_t>(*returned_cmd_id);
    const auto bytes = static_cast<std::uint32_t>(*returned_bytes_read);
    assert_always(id < kLoadCmdTrackerEntries,
                  "Load command tracker response ID is out of range");
    if (id < kLoadCmdTrackerEntries) {
      const auto& entry = q.entries[id];
      assert_always(entry.valid == 1, "Load command tracker response targets a free entry");
      assert_always(bytes <= entry.bytes_left, "Load command tracker response exceeds remaining bytes");
      if (entry.valid == 1 && bytes <= entry.bytes_left) {
        next.entries[id].bytes_left = entry.bytes_left - bytes;
        trace(ld_ctrl_tracker_, "return id=%u bytes=%u left=%u\n",
              static_cast<unsigned>(id), static_cast<unsigned>(bytes),
              static_cast<unsigned>(next.entries[id].bytes_left));
      }
    }
  }

  if (alloc_val == 1 && alloc_rdy == 1) {
    const auto id = static_cast<std::size_t>(*alloc_cmd_id);
    const auto bytes = static_cast<std::uint32_t>(*alloc_bytes_to_read);
    assert_always(bytes != 0, "A load must allocate a nonzero byte count");
    next.entries[id] = LdCtrlCmdTrackerEntry{1, *alloc_rs_tag, bytes};
    trace(ld_ctrl_tracker_, "alloc id=%u tag=%u bytes=%u\n",
          static_cast<unsigned>(id), static_cast<unsigned>(*alloc_rs_tag),
          static_cast<unsigned>(bytes));
  }

  tracker_D_ = next;
}

void LdCtrlCmdTracker::reset() {
  tracker_D_.reset(LdCtrlCmdTrackerState{});
  alloc_rdy.reset(0);
  alloc_cmd_id.reset(0);
  completed_val.reset(0);
  completed_bits.reset(0);
  completed_cmd_id.reset(0);
  busy.reset(0);
}

} // namespace smesh
