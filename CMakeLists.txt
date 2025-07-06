cmake_minimum_required(VERSION 3.25)
project(HOME-SERVER VERSION 1.0.0 LANGUAGES CXX)

# C++23を要求
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# コンパイラフラグ
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# 依存関係の検索
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Crow REQUIRED)

# ソースファイル
set(SOURCES
    src/server/main.cpp
)

# ヘッダーファイル
set(HEADERS
    src/server/auth.hpp
    src/server/middleware.hpp
    src/server/user_manager.hpp
    src/server/user_api.hpp
    src/server/user_routes.hpp
    src/server/viewer.hpp
    src/server/memo.hpp
    src/server/config.hpp
    src/server/headers.hpp
    src/server/server_systemd.hpp
)

# 実行ファイルの作成
add_executable(home-server ${SOURCES} ${HEADERS})

# リンクライブラリ
target_link_libraries(home-server
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    Crow::Crow
)

# インストール設定
install(TARGETS home-server
    RUNTIME DESTINATION bin
)

# ドメイン設定ファイルの自動生成
add_custom_command(
    TARGET home-server PRE_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/generate_domain_config.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating domain configuration file"
)

# 設定ファイルのインストール
install(FILES config/users.json.example
    DESTINATION share/home-server
    RENAME users.json
)

install(FILES config/domain.example
    DESTINATION share/home-server
    RENAME domain.example
)

# systemdサービスファイルのインストール
configure_file(
    ${CMAKE_SOURCE_DIR}/scripts/home-server.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/home-server.service
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/home-server.service
    DESTINATION /etc/systemd/system
)

# テスト設定（将来の拡張用）
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    # テストの設定をここに追加
endif()

# パッケージ設定
set(CPACK_PACKAGE_NAME "home-server")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Web-based home server with image viewer and memo management")
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