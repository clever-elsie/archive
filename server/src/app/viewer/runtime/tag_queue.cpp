#include <app/viewer/manager.hpp>

#include <algorithm>
#include <iterator>

#include <app/viewer/runtime/metadata_writer.hpp>

namespace VIEWER {

bool Manager::has_tag_transactions() {
  std::lock_guard lock(tag_mutex_);
  return !tag_queue_.empty();
}

bool Manager::has_current_graph() {
  std::lock_guard lock(graph_mutex_);
  return current_slot_ >= 0 && states_[static_cast<std::size_t>(current_slot_)].ready;
}

std::future<TagResult> Manager::enqueue_tag(TagTransaction transaction) {
  if (!transaction.completion) transaction.completion = std::make_shared<std::promise<TagResult>>();
  auto future = transaction.completion->get_future();
  {
    std::lock_guard lock(tag_mutex_);
    tag_queue_.push_back(std::move(transaction));
  }
  work_cv_.notify_all();
  return future;
}

TagResult Manager::apply_tag(GraphState& state, const TagTransaction& transaction) {
  TagResult result;
  const auto found = state.index.find(transaction.target_id);
  if (found == state.index.end()) {
    result.code = "STALE_REFERENCE";
    return result;
  }
  const EntryId canonical_id = found->second.canonical_id;
  const auto canonical = state.index.find(canonical_id);
  if (canonical == state.index.end() || canonical->second.node == invalid_node) {
    result.code = "STALE_REFERENCE";
    return result;
  }
  const auto* node = state.get(canonical->second.node);
  if (!node || (node->kind != NodeKind::work && node->kind != NodeKind::collection) ||
      (node->flags & node_attached_media_flag) != 0) {
    result.code = "INVALID_TAG_TARGET";
    return result;
  }
  auto& tags = state.tags[canonical_id];
  const auto previous_tags = tags;
  if (transaction.add) {
    if (std::ranges::find(tags, transaction.tag) == tags.end()) tags.push_back(transaction.tag);
  } else {
    std::erase(tags, transaction.tag);
  }
  bool written = false;
  try {
    written = metadata::write_tags(root_, *node, state, tags);
  } catch (...) {
    written = false;
  }
  if (!written) {
    tags = previous_tags;
    mark_dirty();
    result.code = "METADATA_WRITE_FAILED";
    return result;
  }
  result.success = true;
  result.code = "OK";
  result.canonical_id = canonical_id;
  result.tags = tags;
  return result;
}

void Manager::process_tag_queue() {
  std::vector<TagTransaction> transactions;
  {
    std::lock_guard lock(tag_mutex_);
    if (tag_queue_.empty()) return;
    transactions.assign(std::make_move_iterator(tag_queue_.begin()), std::make_move_iterator(tag_queue_.end()));
    tag_queue_.clear();
  }

  std::unique_lock graph_lock(graph_mutex_);
  if (current_slot_ < 0 || graph_blocked_) {
    const bool stopping = stop_requested_.load(std::memory_order_acquire);
    if (stopping) {
      graph_lock.unlock();
      for (auto& transaction : transactions)
        transaction.completion->set_value(TagResult{false, "NOT_READY", 0, {}});
      return;
    }
    std::lock_guard tag_lock(tag_mutex_);
    for (auto it = transactions.rbegin(); it != transactions.rend(); ++it)
      tag_queue_.push_front(std::move(*it));
    return;
  }
  graph_blocked_ = true;
  graph_cv_.wait(graph_lock, [this] {
    return current_slot_ < 0 || active_readers_[static_cast<std::size_t>(current_slot_)] == 0 ||
           stop_requested_.load(std::memory_order_acquire);
  });
  if (current_slot_ < 0 || stop_requested_.load(std::memory_order_acquire)) {
    graph_blocked_ = false;
    graph_lock.unlock();
    for (auto& transaction : transactions)
      transaction.completion->set_value(TagResult{false, "NOT_READY", 0, {}});
    graph_cv_.notify_all();
    return;
  }
  auto& state = states_[static_cast<std::size_t>(current_slot_)];
  std::vector<TagResult> results;
  results.reserve(transactions.size());
  for (auto& transaction : transactions)
    try {
      results.push_back(apply_tag(state, transaction));
    } catch (...) {
      mark_dirty();
      results.push_back(TagResult{false, "METADATA_WRITE_FAILED", 0, {}});
    }
  graph_blocked_ = false;
  graph_lock.unlock();
  graph_cv_.notify_all();
  for (std::size_t index = 0; index < transactions.size(); ++index)
    transactions[index].completion->set_value(std::move(results[index]));
}

} // namespace VIEWER
