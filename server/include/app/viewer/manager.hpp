#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <crow/http_request.h>

#include <app/viewer/model/graph.hpp>
#include <app/viewer/runtime/read_view.hpp>
#include <app/viewer/runtime/results.hpp>
#include <app/viewer/scanner.hpp>

namespace VIEWER {

enum class ViewerState : std::uint8_t {
  ready,
  reloading,
  unavailable
};

class Manager final {
public:
  static Manager& get_instance();

  Manager(const Manager&) = delete;
  Manager& operator=(const Manager&) = delete;

  void configure(std::filesystem::path root, std::vector<std::string> access_rules,
                 std::chrono::seconds periodic_scan_interval);
  void start_initial_load();
  void shutdown() noexcept;
  void request_stop() noexcept;

  ReloadResult request_reload();
  void mark_dirty();

  ReadView acquire_read();
  void release_read(int slot) noexcept;
  ViewerState viewer_state();

  const std::filesystem::path& root_path() const noexcept { return root_; }
  bool source_available(std::string_view relative_path) const;
  bool source_available(const GraphState& state, NodeRef node) const;
  bool can_access(const GraphState& state, NodeRef node, bool administrator) const;
  bool can_access_alias(const GraphState& state, const AliasRecord& alias, bool administrator) const;
  bool is_admin_request(const crow::request& req) const;

  std::future<TagResult> enqueue_tag(TagTransaction transaction);

private:
  Manager() = default;
  ~Manager() = default;

  void reload_loop();
  bool perform_reload();
  void process_tag_queue();
  bool has_tag_transactions();
  bool has_current_graph();
  bool can_access_path(std::string_view path) const;
  TagResult apply_tag(GraphState& state, const TagTransaction& transaction);

  std::filesystem::path root_;
  std::vector<std::string> access_rules_;

  std::array<GraphState, 2> states_;
  int current_slot_ = -1;
  std::array<std::size_t, 2> active_readers_{0, 0};
  bool graph_blocked_ = false;

  std::mutex graph_mutex_;
  std::condition_variable graph_cv_;

  std::mutex work_mutex_;
  std::condition_variable work_cv_;
  bool reload_pending_ = false;
  bool reload_running_ = false;
  std::atomic_bool dirty_{false};
  std::atomic_bool reload_visible_{false};
  std::atomic_bool stop_requested_{false};
  bool has_reload_started_ = false;
  std::chrono::steady_clock::time_point last_reload_start_{};
  std::chrono::milliseconds minimum_reload_interval_{1000};
  std::chrono::seconds periodic_scan_interval_{std::chrono::hours{3}};
  std::thread worker_;

  std::mutex tag_mutex_;
  std::deque<TagTransaction> tag_queue_;

  friend struct ReadView;
};

// main.cppの起動・停止接点だけを新Managerへ接続する公開alias。
using manager = Manager;

} // namespace VIEWER
