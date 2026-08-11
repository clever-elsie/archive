#include <app/viewer/scan/classifier.hpp>

#include <cctype>
#include <string>

namespace VIEWER::scan {
namespace {

std::string lower_ascii(std::string value) {
  for (char& c : value)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}

} // namespace

MediaType classify(const std::filesystem::path& path, bool& archive) {
  archive = false;
  const auto ext = lower_ascii(path.extension().string());
  if (ext == ".zip") {
    archive = true;
    return MediaType::image;
  }
  if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp" || ext == ".gif")
    return MediaType::image;
  if (ext == ".mp4" || ext == ".mkv" || ext == ".webm")
    return MediaType::video;
  if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".aac" || ext == ".ogg" || ext == ".m4a")
    return MediaType::audio;
  if (ext == ".txt" || ext == ".md")
    return MediaType::text;
  if (ext == ".pdf" || ext == ".doc" || ext == ".docx" || ext == ".odt")
    return MediaType::document;
  return MediaType::unknown;
}

bool is_metadata_file(const std::filesystem::path& path) {
  const auto name = path.filename().generic_string();
  return name == ".viewer.json" || name == ".viewer.json.tmp" || name == ".info";
}

} // namespace VIEWER::scan
