// **********************************************************************
// smesh/src/controllers/st/StCtrlCmdTracker.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026

#include "StCtrlCmdTracker.hpp"

namespace smesh {

TraceKey(st_ctrl_tracker_);

StCtrlCmdTracker::StCtrlCmdTracker(std::string /*name*/, IMPL_CTOR) {
  tracker_Q_ <= tracker_D_;

  UPDATE(updateView)
      .reads(tracker_Q_)
      .writes(alloc_rdy,
              alloc_cmd_id,
              returned_rdy,
              completed_val,
              completed_bits,
              completed_cmd_id);
  UPDATE(updateState)
      .reads(tracker_Q_,
             alloc_val,
             alloc_response_count,
             alloc_rs_tag,
             alloc_rdy,
             alloc_cmd_id,
             returned_val,
             returned_rdy)
      .reads(returned_cmd_id,
             returned_response_count,
             completed_val,
             completed_rdy,
             completed_cmd_id)
      .writes(tracker_D_);
}

void StCtrlCmdTracker::updateView() {
  const auto state = *tracker_Q_;

  bool found_free = false;
  std::size_t free_id = 0;
  bool found_complete = false;
  std::size_t complete_id = 0;

  for (std::size_t i = 0; i < kStoreCmdTrackerEntries; ++i) {
    const auto& entry = state.entries[i];
    if (!found_free && entry.valid == 0) {
      found_free = true;
      free_id = i;
    }
    if (!found_complete && entry.valid == 1 && entry.responses_left == 0) {
      found_complete = true;
      complete_id = i;
    }
  }

  alloc_rdy = bit(found_free);
  alloc_cmd_id = static_cast<std::uint16_t>(free_id);
  returned_rdy = 1;
  completed_val = bit(found_complete);
  completed_cmd_id = static_cast<std::uint16_t>(complete_id);
  completed_bits = found_complete ? state.entries[complete_id].rs_tag : 0;
}

void StCtrlCmdTracker::updateState() {
  const auto state = *tracker_Q_;
  auto next = state;

  const bool completion_fire = completed_val == 1 && completed_rdy == 1;
  const bool returned_fire = returned_val == 1 && returned_rdy == 1;
  const bool alloc_fire = alloc_val == 1 && alloc_rdy == 1;

  if (completion_fire) {
    const auto id = static_cast<std::size_t>(*completed_cmd_id);
    next.entries[id] = StCtrlCmdTrackerEntry{};
    trace(st_ctrl_tracker_,
          "complete id=%u tag=%u\n",
          static_cast<unsigned>(id),
          static_cast<unsigned>(state.entries[id].rs_tag));
  }

  if (returned_fire) {
    const auto id = static_cast<std::size_t>(*returned_cmd_id);
    const auto count = static_cast<std::uint32_t>(*returned_response_count);
    assert_always(id < kStoreCmdTrackerEntries,
                  "Store command tracker response ID is out of range");
    if (id < kStoreCmdTrackerEntries) {
      const auto& entry = state.entries[id];
      assert_always(entry.valid == 1,
                    "Store command tracker response targets a free entry");
      assert_always(count <= entry.responses_left,
                    "Store command tracker response count exceeds remaining count");
      const auto next_count = entry.valid == 1 && count <= entry.responses_left
                                  ? entry.responses_left - count
                                  : entry.responses_left;
      if (entry.valid == 1 && count <= entry.responses_left) {
        next.entries[id].responses_left = entry.responses_left - count;
      }
      trace(st_ctrl_tracker_,
            "return id=%u count=%u left=%u\n",
            static_cast<unsigned>(id),
            static_cast<unsigned>(count),
            static_cast<unsigned>(next_count));
    }
  }

  if (alloc_fire) {
    const auto id = static_cast<std::size_t>(*alloc_cmd_id);
    assert_always(id < kStoreCmdTrackerEntries,
                  "Store command tracker allocation ID is out of range");
    auto& entry = next.entries[id];
    entry.valid = 1;
    entry.rs_tag = *alloc_rs_tag;
    entry.responses_left = static_cast<std::uint32_t>(*alloc_response_count);
    trace(st_ctrl_tracker_,
          "alloc id=%u tag=%u count=%u\n",
          static_cast<unsigned>(id),
          static_cast<unsigned>(entry.rs_tag),
          static_cast<unsigned>(entry.responses_left));
  }

  tracker_D_ = next;
}

void StCtrlCmdTracker::reset() {
  tracker_D_.reset(StCtrlCmdTrackerState{});
  alloc_rdy.reset(0);
  alloc_cmd_id.reset(0);
  returned_rdy.reset(0);
  completed_val.reset(0);
  completed_bits.reset(0);
  completed_cmd_id.reset(0);
}

} // namespace smesh
