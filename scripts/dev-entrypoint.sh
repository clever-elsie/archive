#!/bin/bash

set -e

echo "=== HOME-SERVER Development Environment ==="
echo "Current directory: $(pwd)"
echo "Build type: $CMAKE_BUILD_TYPE"

# ビルドディレクトリの作成
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir -p build
fi

# CMakeの設定
echo "Configuring CMake..."
cd build
cmake .. -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-Debug}

# ビルド
echo "Building project..."
make -j$(nproc)

# 実行ファイルの権限確認
if [ -f "home-server" ]; then
    chmod +x home-server
    echo "Build completed successfully!"
    echo "Starting server..."
    ./home-server
else
    echo "Error: home-server executable not found!"
    exit 1
fi 