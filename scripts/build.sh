#!/bin/bash

set -e

# 色付き出力
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 関数
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 引数の解析
BUILD_TYPE="Release"
CLEAN_BUILD=false
INSTALL=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --install)
            INSTALL=true
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --debug     Build in debug mode"
            echo "  --clean     Clean build directory before building"
            echo "  --install   Install after building"
            echo "  --help      Show this help message"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# makefile.env自動生成（未作成時のみ）
if [ ! -f "makefile.env" ]; then
  echo "makefile.envが存在しないため自動生成します。"
  cat <<EOF > makefile.env
# Makefile用パラメータファイル（自動生成されました）
# サービス名
SERVICE=myservice
# 出力バイナリ名
OUT=server_systemd.out
EOF
fi

print_info "Building HOME-SERVER in $BUILD_TYPE mode"

# 依存関係のチェック
check_dependencies() {
    print_info "Checking dependencies..."
    
    if ! command -v cmake &> /dev/null; then
        print_error "CMake is not installed"
        exit 1
    fi
    
    if ! command -v make &> /dev/null; then
        print_error "Make is not installed"
        exit 1
    fi
    
    if ! pkg-config --exists openssl; then
        print_error "OpenSSL development libraries are not installed"
        exit 1
    fi
    
    # C++23対応コンパイラのチェック
    if command -v g++ &> /dev/null; then
        GCC_VERSION=$(g++ -dumpversion | cut -d. -f1)
        if [ "$GCC_VERSION" -lt 13 ]; then
            print_warning "GCC version $GCC_VERSION detected. C++23 requires GCC 13+"
            print_info "Consider upgrading to GCC 13+ for full C++23 support"
        fi
    fi
    
    if command -v clang++ &> /dev/null; then
        CLANG_VERSION=$(clang++ --version | head -n1 | grep -o '[0-9]\+\.[0-9]\+' | head -n1 | cut -d. -f1)
        if [ "$CLANG_VERSION" -lt 17 ]; then
            print_warning "Clang version $CLANG_VERSION detected. C++23 requires Clang 17+"
            print_info "Consider upgrading to Clang 17+ for full C++23 support"
        fi
    fi
    
    print_info "All dependencies found"
}

# Crowフレームワークの準備
prepare_crow() {
    if [ ! -d "third_party/crow" ]; then
        print_info "Cloning Crow framework..."
        mkdir -p third_party
        git clone https://github.com/CrowCpp/Crow.git third_party/crow
    else
        print_info "Crow framework already exists"
    fi
}

# ビルドディレクトリの準備
prepare_build_dir() {
    if [ "$CLEAN_BUILD" = true ]; then
        print_info "Cleaning build directory..."
        rm -rf build
    fi
    
    mkdir -p build
}

# CMakeの設定
configure_cmake() {
    print_info "Configuring CMake..."
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE
    cd ..
}

# ビルド
build_project() {
    print_info "Building project..."
    cd build
    make -j$(nproc)
    cd ..
}

# インストール
install_project() {
    if [ "$INSTALL" = true ]; then
        print_info "Installing project..."
        cd build
        sudo make install
        cd ..
        
        # systemdユーザーの作成
        if ! id "homeserver" &>/dev/null; then
            print_info "Creating homeserver user..."
            sudo useradd -r -s /bin/false homeserver
        fi
        
        # データディレクトリの作成
        sudo mkdir -p /var/lib/home-server
        sudo chown homeserver:homeserver /var/lib/home-server
        
        print_info "Installation completed"
        print_info "To start the service: sudo systemctl enable home-server && sudo systemctl start home-server"
    fi
}

# メイン処理
main() {
    check_dependencies
    prepare_crow
    prepare_build_dir
    configure_cmake
    build_project
    install_project
    
    print_info "Build completed successfully!"
    print_info "Executable location: build/home-server"
}

# スクリプトの実行
main "$@" 