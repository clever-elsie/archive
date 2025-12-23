cmake_minimum_required(VERSION 3.25)
project(HOME-SERVER VERSION 1.2.1 LANGUAGES CXX)

# C++23を要求
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# gnu++23を使う
set(CMAKE_CXX_EXTENSIONS ON)

# コンパイラフラグ
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -flto=auto -march=native")

# 依存関係の検索
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Crow REQUIRED)
find_package(ZLIB REQUIRED)

# プロジェクト内のインクルードディレクトリ
include_directories(server/include)

# サーバ実装は server/src 以下に集約
file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/server/src/*.cpp")

add_executable(home-server ${SOURCES})

# リンクライブラリ
target_link_libraries(home-server
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    Crow::Crow
    ZLIB::ZLIB
)

target_compile_definitions(home-server PRIVATE CROW_ENABLE_SSL=1)

target_precompile_headers(home-server PRIVATE
    <algorithm> <ranges>
    <array> <vector>
    <string> <string_view>
    <set> <map> <unordered_set> <unordered_map>
    <ext/pb_ds/tree_policy.hpp> <ext/pb_ds/assoc_container.hpp>
    <mutex> <exception>
    <chrono> <filesystem>
    <fstream> <iostream> <sstream>
    <cstddef> <cstdint> <cctype>
    <crow.h> <crow/json.h> <crow/multipart.h>
    <crow/http_response.h> <crow/http_request.h>
    <openssl/ssl.h> <openssl/crypto.h>
    <openssl/hmac.h> <openssl/sha.h> <openssl/rand.h>
    <openssl/bio.h> <openssl/buffer.h> <openssl/evp.h>
)

# パッケージ設定
set(CPACK_PACKAGE_NAME "home-server")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Web-based home server with contents viewer and memo management")
set(CPACK_PACKAGE_VENDOR "HOME-SERVER Project")
set(CPACK_PACKAGE_CONTACT "https://github.com/clever-elsie/archive")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/clever-elsie/archive")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# デバッグ情報の出力
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenSSL Found: ${OPENSSL_FOUND}")
message(STATUS "OpenSSL Version: ${OPENSSL_VERSION}")
message(STATUS "Crow Found: ${Crow_FOUND}") 
message(STATUS "Zlib Found: ${ZLIB_FOUND}")
message(STATUS "Zlib Version: ${ZLIB_VERSION_STRING}")