#include <app/viewer/zip_util.hpp>
#include <fstream>
#include <algorithm>
#include <cstdint>
#include <cctype>
#include <zlib.h>

namespace VIEWER {
namespace zip_util {

namespace {

std::string to_lower(std::string_view sv) {
    std::string res;
    res.reserve(sv.size());
    for (char c : sv) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

bool is_image_extension(const std::filesystem::path& path) {
    std::string ext = to_lower(path.extension().string());
    return (ext == ".webp" || ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif");
}

std::string get_mime_type(const std::filesystem::path& path) {
    std::string ext = to_lower(path.extension().string());
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".webp") return "image/webp";
    if (ext == ".gif") return "image/gif";
    return "application/octet-stream";
}

bool decompress_deflate(const char* compressed_data, size_t compressed_size, std::vector<char>& out_data, size_t uncompressed_size) {
    out_data.resize(uncompressed_size);
    
    z_stream strm{};
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed_data));
    strm.avail_in = static_cast<uInt>(compressed_size);
    strm.next_out = reinterpret_cast<Bytef*>(out_data.data());
    strm.avail_out = static_cast<uInt>(uncompressed_size);

    // raw deflate (windowBits = -15)
    if (inflateInit2(&strm, -15) != Z_OK) {
        return false;
    }

    int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    return (ret == Z_STREAM_END || ret == Z_OK);
}

} // namespace

std::vector<std::string> get_zip_filenames(const std::filesystem::path& zip_path) {
    std::vector<std::string> filenames;
    std::ifstream file(zip_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return filenames;

    const std::streamsize file_size = file.tellg();
    if (file_size < 22) return filenames;

    const size_t search_size = std::min<size_t>(static_cast<size_t>(file_size), 65557);
    file.seekg(file_size - static_cast<std::streamsize>(search_size), std::ios::beg);

    std::vector<char> buffer(search_size);
    if (!file.read(buffer.data(), search_size)) return filenames;

    int eocd_pos = -1;
    for (int i = static_cast<int>(search_size) - 22; i >= 0; --i) {
        if (static_cast<uint8_t>(buffer[i]) == 0x50 &&
            static_cast<uint8_t>(buffer[i+1]) == 0x4b &&
            static_cast<uint8_t>(buffer[i+2]) == 0x05 &&
            static_cast<uint8_t>(buffer[i+3]) == 0x06) {
            eocd_pos = i;
            break;
        }
    }

    if (eocd_pos == -1) return filenames;

    const uint8_t* eocd = reinterpret_cast<const uint8_t*>(&buffer[eocd_pos]);
    const uint16_t num_entries = static_cast<uint16_t>(eocd[10]) | (static_cast<uint16_t>(eocd[11]) << 8);
    const uint32_t cd_offset = static_cast<uint32_t>(eocd[16]) |
                              (static_cast<uint32_t>(eocd[17]) << 8) |
                              (static_cast<uint32_t>(eocd[18]) << 16) |
                              (static_cast<uint32_t>(eocd[19]) << 24);

    if (cd_offset >= static_cast<uint32_t>(file_size)) return filenames;

    file.seekg(cd_offset, std::ios::beg);

    for (uint16_t i = 0; i < num_entries; ++i) {
        uint8_t cd_header[46];
        if (!file.read(reinterpret_cast<char*>(cd_header), 46)) break;

        if (cd_header[0] != 0x50 || cd_header[1] != 0x4b || cd_header[2] != 0x01 || cd_header[3] != 0x02) {
            break;
        }

        const uint16_t filename_len = static_cast<uint16_t>(cd_header[28]) | (static_cast<uint16_t>(cd_header[29]) << 8);
        const uint16_t extra_len    = static_cast<uint16_t>(cd_header[30]) | (static_cast<uint16_t>(cd_header[31]) << 8);
        const uint16_t comment_len  = static_cast<uint16_t>(cd_header[32]) | (static_cast<uint16_t>(cd_header[33]) << 8);

        std::string fname(filename_len, '\0');
        if (!file.read(&fname[0], filename_len)) break;

        filenames.push_back(fname);

        file.seekg(extra_len + comment_len, std::ios::cur);
    }

    return filenames;
}

bool contains_images(const std::filesystem::path& zip_path) {
    const auto filenames = get_zip_filenames(zip_path);
    for (const auto& fn : filenames) {
        if (is_image_extension(std::filesystem::path(fn))) {
            return true;
        }
    }
    return false;
}

std::optional<ZipImageEntry> extract_first_image(const std::filesystem::path& zip_path) {
    std::ifstream file(zip_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return std::nullopt;

    const std::streamsize file_size = file.tellg();
    if (file_size < 22) return std::nullopt;

    const size_t search_size = std::min<size_t>(static_cast<size_t>(file_size), 65557);
    file.seekg(file_size - static_cast<std::streamsize>(search_size), std::ios::beg);

    std::vector<char> buffer(search_size);
    if (!file.read(buffer.data(), search_size)) return std::nullopt;

    int eocd_pos = -1;
    for (int i = static_cast<int>(search_size) - 22; i >= 0; --i) {
        if (static_cast<uint8_t>(buffer[i]) == 0x50 &&
            static_cast<uint8_t>(buffer[i+1]) == 0x4b &&
            static_cast<uint8_t>(buffer[i+2]) == 0x05 &&
            static_cast<uint8_t>(buffer[i+3]) == 0x06) {
            eocd_pos = i;
            break;
        }
    }

    if (eocd_pos == -1) return std::nullopt;

    const uint8_t* eocd = reinterpret_cast<const uint8_t*>(&buffer[eocd_pos]);
    const uint16_t num_entries = static_cast<uint16_t>(eocd[10]) | (static_cast<uint16_t>(eocd[11]) << 8);
    const uint32_t cd_offset = static_cast<uint32_t>(eocd[16]) |
                              (static_cast<uint32_t>(eocd[17]) << 8) |
                              (static_cast<uint32_t>(eocd[18]) << 16) |
                              (static_cast<uint32_t>(eocd[19]) << 24);

    if (cd_offset >= static_cast<uint32_t>(file_size)) return std::nullopt;

    file.seekg(cd_offset, std::ios::beg);

    struct CDItem {
        std::string filename;
        uint16_t comp_method;
        uint32_t comp_size;
        uint32_t uncomp_size;
        uint32_t local_header_offset;
    };
    std::vector<CDItem> image_items;

    for (uint16_t i = 0; i < num_entries; ++i) {
        uint8_t cd_header[46];
        if (!file.read(reinterpret_cast<char*>(cd_header), 46)) break;

        if (cd_header[0] != 0x50 || cd_header[1] != 0x4b || cd_header[2] != 0x01 || cd_header[3] != 0x02) {
            break;
        }

        const uint16_t comp_method = static_cast<uint16_t>(cd_header[10]) | (static_cast<uint16_t>(cd_header[11]) << 8);
        const uint32_t comp_size = static_cast<uint32_t>(cd_header[20]) | (static_cast<uint32_t>(cd_header[21]) << 8) |
                                  (static_cast<uint32_t>(cd_header[22]) << 16) | (static_cast<uint32_t>(cd_header[23]) << 24);
        const uint32_t uncomp_size = static_cast<uint32_t>(cd_header[24]) | (static_cast<uint32_t>(cd_header[25]) << 8) |
                                    (static_cast<uint32_t>(cd_header[26]) << 16) | (static_cast<uint32_t>(cd_header[27]) << 24);
        const uint16_t filename_len = static_cast<uint16_t>(cd_header[28]) | (static_cast<uint16_t>(cd_header[29]) << 8);
        const uint16_t extra_len    = static_cast<uint16_t>(cd_header[30]) | (static_cast<uint16_t>(cd_header[31]) << 8);
        const uint16_t comment_len  = static_cast<uint16_t>(cd_header[32]) | (static_cast<uint16_t>(cd_header[33]) << 8);
        const uint32_t local_offset = static_cast<uint32_t>(cd_header[42]) | (static_cast<uint32_t>(cd_header[43]) << 8) |
                                     (static_cast<uint32_t>(cd_header[44]) << 16) | (static_cast<uint32_t>(cd_header[45]) << 24);

        std::string fname(filename_len, '\0');
        if (!file.read(&fname[0], filename_len)) break;

        if (is_image_extension(std::filesystem::path(fname))) {
            image_items.push_back({fname, comp_method, comp_size, uncomp_size, local_offset});
        }

        file.seekg(extra_len + comment_len, std::ios::cur);
    }

    if (image_items.empty()) return std::nullopt;

    // ファイル名で並べ替えて1枚目を特定
    std::sort(image_items.begin(), image_items.end(), [](const CDItem& a, const CDItem& b) {
        return a.filename < b.filename;
    });

    const auto& target = image_items.front();

    // Local Header に移動
    file.seekg(target.local_header_offset, std::ios::beg);
    uint8_t local_header[30];
    if (!file.read(reinterpret_cast<char*>(local_header), 30)) return std::nullopt;

    if (local_header[0] != 0x50 || local_header[1] != 0x4b || local_header[2] != 0x03 || local_header[3] != 0x04) {
        return std::nullopt;
    }

    const uint16_t local_fname_len = static_cast<uint16_t>(local_header[26]) | (static_cast<uint16_t>(local_header[27]) << 8);
    const uint16_t local_extra_len = static_cast<uint16_t>(local_header[28]) | (static_cast<uint16_t>(local_header[29]) << 8);

    // 圧縮データ開始オフセットに移動
    file.seekg(target.local_header_offset + 30 + local_fname_len + local_extra_len, std::ios::beg);

    std::vector<char> comp_buffer(target.comp_size);
    if (!file.read(comp_buffer.data(), target.comp_size)) return std::nullopt;

    ZipImageEntry result;
    result.filename = target.filename;
    result.mime_type = get_mime_type(std::filesystem::path(target.filename));

    if (target.comp_method == 0) { // Stored (無圧縮)
        result.data = std::move(comp_buffer);
        return result;
    } else if (target.comp_method == 8) { // Deflated
        if (decompress_deflate(comp_buffer.data(), comp_buffer.size(), result.data, target.uncomp_size)) {
            return result;
        }
    }

    return std::nullopt;
}

} // namespace zip_util
} // namespace VIEWER
