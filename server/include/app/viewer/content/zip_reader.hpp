#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace VIEWER::content::zip_reader {

struct Entry final {
  std::string name;
  std::uint16_t compression_method = 0;
  std::uint32_t compressed_size = 0;
  std::uint32_t uncompressed_size = 0;
  std::uint32_t local_header_offset = 0;
};

std::vector<Entry> list_entries(const std::filesystem::path& archive);
std::optional<std::vector<char>> extract_entry(const std::filesystem::path& archive, const Entry& entry);
bool is_image_entry(const std::string& name);
std::string mime_type(const std::string& name);

} // namespace VIEWER::content::zip_reader
