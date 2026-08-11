#pragma once

#include <cstdint>

namespace VIEWER {

using EntryId = std::uint64_t;

enum class MediaType : std::uint8_t {
  image,
  video,
  audio,
  text,
  document,
  unknown
};

constexpr std::uint32_t media_type_bit(MediaType type) noexcept {
  switch (type) {
    case MediaType::image: return 1u << 0;
    case MediaType::video: return 1u << 1;
    case MediaType::audio: return 1u << 2;
    case MediaType::text: return 1u << 3;
    case MediaType::document: return 1u << 4;
    case MediaType::unknown: return 0;
  }
  return 0;
}

// media-setのroleではない。子ディレクトリをCollectionとして解釈するか、
// 複数のMediaSetを持つWorkとして解釈するかが構造だけでは曖昧な場合にだけ使う。
enum class DeclaredNodeKind : std::uint8_t {
  unspecified,
  collection,
  work,
  media_set
};

} // namespace VIEWER
