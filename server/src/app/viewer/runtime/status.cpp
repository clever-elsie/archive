#include <app/viewer/manager.hpp>

namespace VIEWER {

ViewerState Manager::viewer_state() {
  std::lock_guard lock(graph_mutex_);
  if (graph_blocked_ || reload_visible_.load(std::memory_order_acquire)) return ViewerState::reloading;
  if (current_slot_ >= 0 && states_[static_cast<std::size_t>(current_slot_)].ready)
    return ViewerState::ready;
  return ViewerState::unavailable;
}

} // namespace VIEWER
