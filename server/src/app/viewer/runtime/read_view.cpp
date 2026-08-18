#include <app/viewer/manager.hpp>

namespace VIEWER {

ReadView& ReadView::operator=(ReadView&& other) noexcept {
  if (this == &other) return *this;
  if (owner_) owner_->release_read(slot_);
  owner_ = other.owner_;
  slot_ = other.slot_;
  state_ = other.state_;
  other.owner_ = nullptr;
  other.slot_ = -1;
  other.state_ = nullptr;
  return *this;
}

ReadView::~ReadView() {
  if (owner_) owner_->release_read(slot_);
}

ReadView Manager::acquire_read() {
  std::unique_lock lock(graph_mutex_);
  graph_cv_.wait(lock, [this] {
    return !graph_blocked_ || current_slot_ < 0 || stop_requested_.load(std::memory_order_acquire);
  });
  if (current_slot_ < 0 || stop_requested_.load(std::memory_order_acquire)) return {};
  ++active_readers_[static_cast<std::size_t>(current_slot_)];
  return ReadView(this, current_slot_, &states_[static_cast<std::size_t>(current_slot_)]);
}

void Manager::release_read(int slot) noexcept {
  if (slot < 0 || slot >= static_cast<int>(active_readers_.size())) return;
  std::lock_guard lock(graph_mutex_);
  auto& count = active_readers_[static_cast<std::size_t>(slot)];
  if (count > 0) --count;
  graph_cv_.notify_all();
}

} // namespace VIEWER
