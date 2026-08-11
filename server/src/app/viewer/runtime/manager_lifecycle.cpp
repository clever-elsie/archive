#include <app/viewer/manager.hpp>

namespace VIEWER {

Manager& Manager::get_instance() {
  static Manager manager;
  return manager;
}

void Manager::configure(std::filesystem::path root, std::vector<std::string> access_rules,
                        std::chrono::seconds periodic_scan_interval) {
  std::lock_guard lock(work_mutex_);
  root_ = std::move(root);
  access_rules_ = std::move(access_rules);
  periodic_scan_interval_ = periodic_scan_interval;
}

void Manager::start_initial_load() {
  {
    std::lock_guard lock(work_mutex_);
    if (worker_.joinable() || stop_requested_.load(std::memory_order_acquire)) return;
    // 初回も通常のfilesystem走査でGraphStateを構築する。currentが存在しない
    // ため、走査完了まではviewer APIをunavailableとして扱う。
    reload_pending_ = true;
    reload_visible_.store(true, std::memory_order_release);
    worker_ = std::thread([this] { reload_loop(); });
  }
  work_cv_.notify_all();
}

void Manager::request_stop() noexcept {
  stop_requested_.store(true, std::memory_order_release);
  work_cv_.notify_all();
  graph_cv_.notify_all();
}

void Manager::shutdown() noexcept {
  request_stop();
  if (worker_.joinable()) worker_.join();
}

} // namespace VIEWER
