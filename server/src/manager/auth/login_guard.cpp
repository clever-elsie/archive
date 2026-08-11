#include <manager/auth/login_guard.hpp>

#include <algorithm>
#include <limits>

namespace AUTH {

LoginAttemptGuard& login_attempt_guard() {
  static LoginAttemptGuard guard;
  return guard;
}

bool LoginAttemptGuard::allowed(
    const std::string& username,
    const std::string& remote_ip,
    std::uint64_t* retry_after_seconds) {
  std::lock_guard lock(mutex);
  const auto key = std::make_pair(username, remote_ip);
  const auto it = states.find(key);
  if (it == states.end()) {
    if (retry_after_seconds) *retry_after_seconds = 0;
    return true;
  }

  const auto now = Clock::now();
  if (now - it->second.last_failure > STATE_RETENTION) {
    states.erase(it);
    if (retry_after_seconds) *retry_after_seconds = 0;
    return true;
  }
  if (now >= it->second.blocked_until) {
    if (retry_after_seconds) *retry_after_seconds = 0;
    return true;
  }

  const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
      it->second.blocked_until - now);
  const auto seconds = static_cast<std::uint64_t>(std::max<std::int64_t>(
      1, remaining.count()));
  if (retry_after_seconds) *retry_after_seconds = seconds;
  return false;
}

void LoginAttemptGuard::record_failure(
    const std::string& username,
    const std::string& remote_ip) {
  std::lock_guard lock(mutex);
  auto& state = states[std::make_pair(username, remote_ip)];
  state.failures = std::min<std::uint32_t>(MAX_FAILURES, state.failures + 1);

  std::uint64_t delay = INITIAL_DELAY_SECONDS;
  for (std::uint32_t i = 1; i < state.failures && delay < MAX_DELAY_SECONDS; ++i)
    delay = std::min<std::uint64_t>(MAX_DELAY_SECONDS, delay * 2);
  state.blocked_until = Clock::now() + std::chrono::seconds(delay);
  state.last_failure = Clock::now();
}

void LoginAttemptGuard::record_success(
    const std::string& username,
    const std::string& remote_ip) {
  std::lock_guard lock(mutex);
  states.erase(std::make_pair(username, remote_ip));
}

void LoginAttemptGuard::reset_user(const std::string& username) {
  std::lock_guard lock(mutex);
  for (auto it = states.begin(); it != states.end();) {
    if (it->first.first == username)
      it = states.erase(it);
    else
      ++it;
  }
}

} // namespace AUTH
