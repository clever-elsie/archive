#ifndef VIEWER_ZIP_UTIL_HPP
#define VIEWER_ZIP_UTIL_HPP

#include <filesystem>
#include <vector>
#include <string>
#include <optional>

namespace VIEWER {
namespace zip_util {

struct ZipImageEntry {
    std::string filename;
    std::vector<char> data;
    std::string mime_type;
};

// zipファイル内部のファイル相対パス一覧を（非解凍で）高速取得
std::vector<std::string> get_zip_filenames(const std::filesystem::path& zip_path);

// zipファイル内部に画像ファイルが含まれているか判定
bool contains_images(const std::filesystem::path& zip_path);

// zipファイル内の最初の画像ファイルを展開・取得
std::optional<ZipImageEntry> extract_first_image(const std::filesystem::path& zip_path);

} // namespace zip_util
} // namespace VIEWER

#endif
