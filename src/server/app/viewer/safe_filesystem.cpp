#include <app/viewer/safe_filesystem.hpp>
#include <iostream>

namespace VIEWER {

namespace SafeFS {

Result<std::filesystem::file_time_type> 
last_write_time(const std::filesystem::path& p) {
    std::error_code ec;
    auto time = std::filesystem::last_write_time(p, ec);
    return {time, ec};
}

Result<std::filesystem::directory_iterator>
directory_iterator(const std::filesystem::path& p) {
    std::error_code ec;
    auto iter = std::filesystem::directory_iterator(p, ec);
    return {std::move(iter), ec};
}

Result<bool> exists(const std::filesystem::path& p) {
    std::error_code ec;
    bool result = std::filesystem::exists(p, ec);
    return {result, ec};
}

Result<bool> is_directory(const std::filesystem::path& p) {
    std::error_code ec;
    bool result = std::filesystem::is_directory(p, ec);
    return {result, ec};
}

Result<bool> is_regular_file(const std::filesystem::path& p) {
    std::error_code ec;
    bool result = std::filesystem::is_regular_file(p, ec);
    return {result, ec};
}

} // namespace SafeFS

} // namespace VIEWER
