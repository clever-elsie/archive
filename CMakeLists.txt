cmake_minimum_required(VERSION 3.25)
project(HOME-SERVER VERSION 1.0.0 LANGUAGES CXX)

# C++23を要求
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# gnu++23を使う
set(CMAKE_CXX_EXTENSIONS ON)

# コンパイラフラグ
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# 依存関係の検索
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Crow REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(includes)

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/server/*.cpp")

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