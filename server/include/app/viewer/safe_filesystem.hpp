#ifndef SAFE_FILESYSTEM_HPP
#define SAFE_FILESYSTEM_HPP

#include <filesystem>
#include <system_error>
#include <optional>
#include <vector>

namespace VIEWER {

namespace SafeFS {
    // 結果を表す構造体
    template<typename T>
    struct Result {
        T value;
        std::error_code ec;
        bool success() const { return !ec; }
    };
    
    // 安全なlast_write_time取得
    Result<std::filesystem::file_time_type> 
    last_write_time(const std::filesystem::path& p);
    
    // 安全なdirectory_iterator作成
    Result<std::filesystem::directory_iterator>
    directory_iterator(const std::filesystem::path& p);
    
    // 安全なexists確認
    Result<bool> exists(const std::filesystem::path& p);
    
    // 安全なis_directory確認
    Result<bool> is_directory(const std::filesystem::path& p);
    
    // 安全なis_regular_file確認
    Result<bool> is_regular_file(const std::filesystem::path& p);
}

} // namespace VIEWER

#endif
