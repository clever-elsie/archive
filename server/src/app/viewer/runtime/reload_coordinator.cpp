#include <app/viewer/manager.hpp>

#include <algorithm>

#include <crow/logging.h>

#include <app/viewer/graph/builder.hpp>
#include <app/viewer/scanner.hpp>

namespace VIEWER {

ReloadResult Manager::request_reload() {
  std::lock_guard lock(work_mutex_);
  if (stop_requested_.load(std::memory_order_acquire))
    return {ReloadResult::Code::not_ready, std::chrono::milliseconds{0}};
  if (reload_pending_ || reload_running_)
    return {ReloadResult::Code::already_pending, std::chrono::milliseconds{0}};

  const auto now = std::chrono::steady_clock::now();
  if (has_reload_started_ && now - last_reload_start_ < minimum_reload_interval_) {
    return {ReloadResult::Code::cooldown,
            std::chrono::duration_cast<std::chrono::milliseconds>(minimum_reload_interval_ - (now - last_reload_start_))};
  }
  reload_pending_ = true;
  reload_visible_.store(true, std::memory_order_release);
  work_cv_.notify_all();
  return {ReloadResult::Code::accepted, std::chrono::milliseconds{0}};
}

void Manager::mark_dirty() {
  {
    std::lock_guard lock(work_mutex_);
    dirty_ = true;
  }
  work_cv_.notify_all();
}

bool Manager::perform_reload() {
  std::unique_lock graph_lock(graph_mutex_);
  const int buffer_slot = current_slot_ < 0 ? 0 : 1 - current_slot_;
  graph_cv_.wait(graph_lock, [this, buffer_slot] {
    return active_readers_[static_cast<std::size_t>(buffer_slot)] == 0 ||
           stop_requested_.load(std::memory_order_acquire);
  });
  if (stop_requested_.load(std::memory_order_acquire)) {
    graph_lock.unlock();
    graph_cv_.notify_all();
    return false;
  }
  states_[static_cast<std::size_t>(buffer_slot)].clear();
  graph_lock.unlock();

  const auto abort_reload = [&]() {
    graph_lock.lock();
    graph_lock.unlock();
    graph_cv_.notify_all();
    return false;
  };
  auto& candidate = states_[static_cast<std::size_t>(buffer_slot)];

  try {
    const auto snapshot = Scanner::scan(root_, &stop_requested_);
    for (const auto& diagnostic : snapshot.diagnostics) {
      const auto path = diagnostic.relative_path.generic_string();
      if (diagnostic.level == ScanDiagnostic::Level::error)
        CROW_LOG_ERROR << "viewer scan: " << path << ": " << diagnostic.message;
      else
        CROW_LOG_WARNING << "viewer scan: " << path << ": " << diagnostic.message;
    }
    if (!snapshot.complete) return abort_reload();
    build_graph(candidate, root_, snapshot, &stop_requested_);
    for (std::size_t index = snapshot.diagnostics.size(); index < candidate.diagnostics.size(); ++index) {
      const auto& diagnostic = candidate.diagnostics[index];
      CROW_LOG_WARNING << "viewer graph: " << diagnostic.relative_path.generic_string()
                       << ": " << diagnostic.message;
    }
  } catch (const std::exception& exception) {
    CROW_LOG_ERROR << "viewer reload exception: " << exception.what();
    return abort_reload();
  } catch (...) {
    CROW_LOG_ERROR << "viewer reload exception: unknown error";
    return abort_reload();
  }

  if (!candidate.ready || stop_requested_.load(std::memory_order_acquire)) return abort_reload();

  graph_lock.lock();
  if (stop_requested_.load(std::memory_order_acquire)) {
    graph_lock.unlock();
    graph_cv_.notify_all();
    return false;
  }
  current_slot_ = buffer_slot;
  graph_lock.unlock();
  graph_cv_.notify_all();
  CROW_LOG_INFO << "viewer graph ready: nodes=" << candidate.arena.nodes.size()
               << ", aliases=" << candidate.arena.aliases.size()
               << ", diagnostics=" << candidate.diagnostics.size();
  return true;
}

void Manager::reload_loop() {
  std::unique_lock lock(work_mutex_);
  auto next_periodic = std::chrono::steady_clock::now() + periodic_scan_interval_;
  while (!stop_requested_.load(std::memory_order_acquire)) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= next_periodic) {
      // dirty is only an invalidation latch. Automatic reloads are opened by
      // the periodic gate, never by observing dirty in the ordinary loop.
      if (!reload_pending_ && !reload_running_) {
        dirty_ = true;
        reload_pending_ = true;
      }
      next_periodic = now + periodic_scan_interval_;
    }

    const bool reload_allowed = !has_reload_started_ || now - last_reload_start_ >= minimum_reload_interval_;

    if (reload_pending_ && !reload_running_ && reload_allowed) {
      reload_pending_ = false;
      reload_running_ = true;
      reload_visible_.store(true, std::memory_order_release);
      dirty_ = false;
      has_reload_started_ = true;
      last_reload_start_ = now;
      lock.unlock();
      const bool success = perform_reload();
      reload_visible_.store(false, std::memory_order_release);
      if (!success) CROW_LOG_WARNING << "viewer reload failed";
      process_tag_queue();
      lock.lock();
      reload_running_ = false;
      if (!success) dirty_ = true;
      next_periodic = std::chrono::steady_clock::now() + periodic_scan_interval_;
      work_cv_.notify_all();
      continue;
    }

    const bool can_process_tags = !reload_running_ && has_current_graph() && has_tag_transactions();
    if (can_process_tags) {
      lock.unlock();
      process_tag_queue();
      lock.lock();
      continue;
    }

    auto wake_at = next_periodic;
    if (reload_pending_ && has_reload_started_)
      wake_at = std::min(wake_at, last_reload_start_ + minimum_reload_interval_);
    work_cv_.wait_until(lock, wake_at);
  }

  lock.unlock();
  process_tag_queue();
}

} // namespace VIEWER
