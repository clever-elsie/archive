#pragma once

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include <app/viewer/model/types.hpp>

namespace VIEWER {

struct ReloadResult final {
  enum class Code : std::uint8_t {
    accepted,
    already_pending,
    cooldown,
    not_ready
  };
  Code code = Code::not_ready;
  std::chrono::milliseconds retry_after{0};
};

struct TagResult final {
  bool success = false;
  std::string code;
  EntryId canonical_id = 0;
  std::vector<std::string> tags;
};

struct TagTransaction final {
  EntryId target_id = 0;
  bool add = true;
  std::string tag;
  std::shared_ptr<std::promise<TagResult>> completion;
};

} // namespace VIEWER
