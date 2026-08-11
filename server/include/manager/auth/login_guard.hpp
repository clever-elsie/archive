#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

namespace AUTH {

class LoginAttemptGuard {
  using Clock = std::chrono::steady_clock;
  struct State {
    std::uint32_t failures = 0;
    Clock::time_point blocked_until{};
    Clock::time_point last_failure{};
  };

  std::map<std::pair<std::string, std::string>, State> states;
  std::mutex mutex;

  static constexpr std::uint32_t MAX_FAILURES = 31;
  static constexpr std::uint64_t INITIAL_DELAY_SECONDS = 1;
  static constexpr std::uint64_t MAX_DELAY_SECONDS = 60 * 60;
  static constexpr auto STATE_RETENTION = std::chrono::hours(24);

public:
  bool allowed(
      const std::string& username,
      const std::string& remote_ip,
      std::uint64_t* retry_after_seconds = nullptr);
  void record_failure(const std::string& username, const std::string& remote_ip);
  void record_success(const std::string& username, const std::string& remote_ip);
  void reset_user(const std::string& username);
};

LoginAttemptGuard& login_attempt_guard();

} // namespace AUTH
