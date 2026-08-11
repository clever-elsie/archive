#include <app/viewer/content/zip_reader.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>

#include <app/viewer/ordering.hpp>

#include <zlib.h>

namespace VIEWER::content::zip_reader {
namespace {

std::string lower(std::string value) {
  for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}

std::uint16_t u16(const unsigned char* p) {
  return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}

std::uint32_t u32(const unsigned char* p) {
  return static_cast<std::uint32_t>(p[0]) |
         (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) |
         (static_cast<std::uint32_t>(p[3]) << 24);
}

bool inflate_raw(const std::vector<char>& compressed, std::vector<char>& output, std::size_t size) {
  if (size > 256u * 1024u * 1024u || compressed.size() > 256u * 1024u * 1024u) return false;
  output.resize(size);
  z_stream stream{};
  stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
  stream.avail_in = static_cast<uInt>(compressed.size());
  stream.next_out = reinterpret_cast<Bytef*>(output.data());
  stream.avail_out = static_cast<uInt>(output.size());
  if (inflateInit2(&stream, -15) != Z_OK) return false;
  const int result = inflate(&stream, Z_FINISH);
  inflateEnd(&stream);
  return result == Z_STREAM_END || result == Z_OK;
}

} // namespace

bool is_image_entry(const std::string& name) {
  const auto ext = lower(std::filesystem::path(name).extension().string());
  return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp" || ext == ".gif";
}

std::string mime_type(const std::string& name) {
  const auto ext = lower(std::filesystem::path(name).extension().string());
  if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
  if (ext == ".png") return "image/png";
  if (ext == ".webp") return "image/webp";
  if (ext == ".gif") return "image/gif";
  return "application/octet-stream";
}

std::vector<Entry> list_entries(const std::filesystem::path& archive) {
  std::vector<Entry> result;
  std::ifstream file(archive, std::ios::binary | std::ios::ate);
  if (!file) return result;
  const auto size = file.tellg();
  if (size < 22) return result;
  const auto search_size = std::min<std::streamoff>(size, 65557);
  file.seekg(size - search_size);
  std::vector<char> tail(static_cast<std::size_t>(search_size));
  if (!file.read(tail.data(), static_cast<std::streamsize>(tail.size()))) return result;

  std::ptrdiff_t eocd = -1;
  for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(tail.size()) - 22; i >= 0; --i) {
    const auto* p = reinterpret_cast<const unsigned char*>(tail.data() + i);
    if (p[0] == 0x50 && p[1] == 0x4b && p[2] == 0x05 && p[3] == 0x06) {
      eocd = i;
      break;
    }
  }
  if (eocd < 0) return result;
  const auto* header = reinterpret_cast<const unsigned char*>(tail.data() + eocd);
  const auto count = u16(header + 10);
  const auto central_offset = u32(header + 16);
  if (central_offset >= static_cast<std::uint64_t>(size)) return result;

  file.seekg(central_offset);
  for (std::uint32_t i = 0; i < count; ++i) {
    unsigned char central[46]{};
    if (!file.read(reinterpret_cast<char*>(central), sizeof(central))) break;
    if (central[0] != 0x50 || central[1] != 0x4b || central[2] != 0x01 || central[3] != 0x02) break;
    const auto name_size = u16(central + 28);
    const auto extra_size = u16(central + 30);
    const auto comment_size = u16(central + 32);
    std::string name(name_size, '\0');
    if (!file.read(name.data(), static_cast<std::streamsize>(name.size()))) break;
    result.push_back({std::move(name), u16(central + 10), u32(central + 20), u32(central + 24), u32(central + 42)});
    file.seekg(static_cast<std::streamoff>(extra_size) + comment_size, std::ios::cur);
  }
  std::ranges::sort(result, [](const Entry& a, const Entry& b) {
    return ordering::compare_path(a.name, b.name) < 0;
  });
  return result;
}

std::optional<std::vector<char>> extract_entry(const std::filesystem::path& archive, const Entry& entry) {
  std::ifstream file(archive, std::ios::binary);
  if (!file || entry.compressed_size > 256u * 1024u * 1024u || entry.uncompressed_size > 256u * 1024u * 1024u)
    return std::nullopt;
  file.seekg(entry.local_header_offset);
  unsigned char local[30]{};
  if (!file.read(reinterpret_cast<char*>(local), sizeof(local))) return std::nullopt;
  if (local[0] != 0x50 || local[1] != 0x4b || local[2] != 0x03 || local[3] != 0x04) return std::nullopt;
  const auto name_size = u16(local + 26);
  const auto extra_size = u16(local + 28);
  file.seekg(static_cast<std::streamoff>(name_size) + extra_size, std::ios::cur);
  std::vector<char> compressed(entry.compressed_size);
  if (!file.read(compressed.data(), static_cast<std::streamsize>(compressed.size()))) return std::nullopt;
  if (entry.compression_method == 0) {
    if (compressed.size() != entry.uncompressed_size) return std::nullopt;
    return compressed;
  }
  if (entry.compression_method != 8) return std::nullopt;
  std::vector<char> output;
  if (!inflate_raw(compressed, output, entry.uncompressed_size)) return std::nullopt;
  return output;
}

} // namespace VIEWER::content::zip_reader
