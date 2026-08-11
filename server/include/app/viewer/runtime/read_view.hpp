#pragma once

#include <app/viewer/model/graph.hpp>

namespace VIEWER {

class Manager;

struct ReadView final {
  ReadView() = default;
  ReadView(Manager* owner, int slot, const GraphState* state) noexcept
      : owner_(owner), slot_(slot), state_(state) {}
  ReadView(const ReadView&) = delete;
  ReadView& operator=(const ReadView&) = delete;
  ReadView(ReadView&& other) noexcept
      : owner_(other.owner_), slot_(other.slot_), state_(other.state_) {
    other.owner_ = nullptr;
    other.state_ = nullptr;
    other.slot_ = -1;
  }
  ReadView& operator=(ReadView&& other) noexcept;
  ~ReadView();

  explicit operator bool() const noexcept { return state_ != nullptr; }
  const GraphState& state() const noexcept { return *state_; }
  const NodeRecord* node(NodeRef ref) const noexcept { return state_ ? state_->get(ref) : nullptr; }

private:
  Manager* owner_ = nullptr;
  int slot_ = -1;
  const GraphState* state_ = nullptr;
  friend class Manager;
};

} // namespace VIEWER
