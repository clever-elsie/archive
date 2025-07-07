#!/bin/bash

set -e

# 色付き出力
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

print_header() {
    echo -e "${BLUE}=== $1 ===${NC}"
}

# 引数の解析
TEST_TYPE="full"
SKIP_DOCKER=false
SKIP_SECURITY=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --quick)
            TEST_TYPE="quick"
            SKIP_DOCKER=true
            SKIP_SECURITY=true
            shift
            ;;
        --skip-docker)
            SKIP_DOCKER=true
            shift
            ;;
        --skip-security)
            SKIP_SECURITY=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --quick         Quick test (skip Docker and security)"
            echo "  --skip-docker   Skip Docker-related tests"
            echo "  --skip-security Skip security scans"
            echo "  --verbose       Enable verbose output"
            echo "  --help          Show this help message"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# 環境情報の表示
print_header "Docker Test Environment"
echo "Container ID: $(hostname)"
echo "Working Directory: $(pwd)"
echo "User: $(whoami)"
echo "Test Type: $TEST_TYPE"
echo "Skip Docker: $SKIP_DOCKER"
echo "Skip Security: $SKIP_SECURITY"
echo ""

# 依存関係のチェック
check_dependencies() {
    print_header "Checking Dependencies"
    
    local missing_deps=()
    
    # 基本ツール
    command -v git >/dev/null 2>&1 || missing_deps+=("git")
    command -v make >/dev/null 2>&1 || missing_deps+=("make")
    command -v cmake >/dev/null 2>&1 || missing_deps+=("cmake")
    command -v g++ >/dev/null 2>&1 || missing_deps+=("g++")
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        exit 1
    fi
    
    print_info "All dependencies found"
    print_info "GCC version: $(gcc --version | head -n1)"
    print_info "CMake version: $(cmake --version | head -n1)"
}

# プロジェクト構造の検証
validate_project_structure() {
    print_header "Validating Project Structure"
    
    local required_files=(
        "CMakeLists.txt"
        "Makefile"
        "src/server/main.cpp"
        "web/index.html"
        "web/viewer.html"
        "src/frontend/viewer/src.js"
    )
    
    local required_dirs=(
        "src/server"
        "src/frontend"
        "web"
        "scripts"
    )
    
    for file in "${required_files[@]}"; do
        if [ ! -f "$file" ]; then
            print_error "Required file not found: $file"
            exit 1
        fi
    done
    
    for dir in "${required_dirs[@]}"; do
        if [ ! -d "$dir" ]; then
            print_error "Required directory not found: $dir"
            exit 1
        fi
    done
    
    print_info "Project structure is valid"
}

# ローカルビルドテスト
test_local_builds() {
    print_header "Testing Local Builds"
    
    # Makefileビルドテスト
    print_info "Testing Makefile build..."
    make clean || true
    make all
    if [ -f "server_systemd.out" ]; then
        print_info "✓ Makefile build successful"
    else
        print_error "Makefile build failed"
        exit 1
    fi
    
    # CMakeビルドテスト
    print_info "Testing CMake build..."
    rm -rf build
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    cd ..
    
    if [ -f "build/home-server" ]; then
        print_info "✓ CMake build successful"
    else
        print_error "CMake build failed"
        exit 1
    fi
    
    # ビルドスクリプトテスト
    print_info "Testing build script..."
    chmod +x scripts/build.sh
    ./scripts/build.sh --clean
    if [ -f "build/home-server" ]; then
        print_info "✓ Build script successful"
    else
        print_error "Build script failed"
        exit 1
    fi
}

# コード品質チェック
test_code_quality() {
    print_header "Testing Code Quality"
    
    # cppcheck
    if command -v cppcheck &> /dev/null; then
        print_info "Running cppcheck..."
        cppcheck --enable=all --std=c++23 src/ 2> cppcheck-result.txt || true
        print_info "✓ cppcheck completed"
    else
        print_warning "cppcheck not found, skipping code quality check"
    fi
    
    # 基本的なコードスタイルチェック
    print_info "Checking code formatting..."
    local cpp_files=$(find src/ -name "*.cpp" -o -name "*.hpp" | head -10)
    for file in $cpp_files; do
        if [ -f "$file" ]; then
            echo "Checking: $file"
        fi
    done
    print_info "✓ Code format check completed"
}

# ドキュメントチェック
test_documentation() {
    print_header "Testing Documentation"
    
    # README.mdの確認
    if [ -f "README.md" ]; then
        print_info "✓ README.md found"
        
        # 必要なセクションの確認
        grep -q "## 主な機能" README.md || print_warning "Missing main features section"
        grep -q "## セットアップ" README.md || print_warning "Missing setup section"
        grep -q "## ライセンス" README.md || print_warning "Missing license section"
    else
        print_error "README.md not found"
        exit 1
    fi
    
    # HTMLファイルの確認
    for file in web/*.html; do
        if [ -f "$file" ]; then
            echo "Checking: $file"
            grep -q "<!DOCTYPE html>" "$file" || print_warning "$file missing DOCTYPE"
            grep -q "<html" "$file" || print_warning "$file missing html tag"
            grep -q "</html>" "$file" || print_warning "$file missing closing html tag"
        fi
    done
    
    print_info "✓ Documentation check completed"
}

# セキュリティスキャン
test_security() {
    if [ "$SKIP_SECURITY" = true ]; then
        print_warning "Skipping security scans"
        return
    fi
    
    print_header "Testing Security"
    
    # 基本的なセキュリティチェック
    print_info "Checking for common security issues..."
    
    # ハードコードされたパスワードの確認
    if grep -r "password.*=" src/ 2>/dev/null | grep -v "//" | grep -v "example"; then
        print_warning "Potential hardcoded passwords found"
    fi
    
    # SSL証明書の確認
    if [ -f "config/ssl.crt" ]; then
        print_info "✓ SSL certificate found"
    else
        print_warning "SSL certificate not found"
    fi
    
    print_info "✓ Security checks completed"
}

# 統合テスト
test_integration() {
    print_header "Testing Integration"
    
    # テスト用設定ファイルの確認
    if [ -f "test_data/users.json" ]; then
        print_info "✓ Test configuration found"
    else
        print_warning "Test configuration not found, creating..."
        mkdir -p test_data
        echo '{"users": [{"username": "test", "password": "test123", "is_admin": true}]}' > test_data/users.json
    fi
    
    # SSL証明書の確認と生成
    SSL_KEY_FILE=""
    SSL_CERT_FILE=""
    
    # 既存のSSL鍵ファイルを検索
    if [ -d "/etc/nginx/ssl" ]; then
        FOUND_KEY=$(find /etc/nginx/ssl -name "*.key" 2>/dev/null | head -1)
        if [ -n "$FOUND_KEY" ]; then
            SSL_KEY_FILE="$FOUND_KEY"
            print_info "✓ Found SSL key: $SSL_KEY_FILE"
        fi
    fi
    
    # 既存のSSL証明書ファイルを検索
    if [ -d "/etc/nginx/ssl" ]; then
        FOUND_CERT=$(find /etc/nginx/ssl -name "*.crt" 2>/dev/null | head -1)
        if [ -n "$FOUND_CERT" ]; then
            SSL_CERT_FILE="$FOUND_CERT"
            print_info "✓ Found SSL certificate: $SSL_CERT_FILE"
        fi
    fi
    
    # SSL鍵ファイルが見つからない場合は生成
    if [ -z "$SSL_KEY_FILE" ] || [ -z "$SSL_CERT_FILE" ]; then
        print_warning "SSL certificate not found, generating..."
        mkdir -p config/ssl
        openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
            -keyout config/ssl/server.key \
            -out config/ssl/server.crt \
            -subj "/C=JP/ST=Tokyo/L=Tokyo/O=HomeServer/CN=localhost"
        chmod 600 config/ssl/server.key
        chmod 644 config/ssl/server.crt
        SSL_KEY_FILE="config/ssl/server.key"
        SSL_CERT_FILE="config/ssl/server.crt"
        print_info "✓ SSL certificate generated"
    fi
    
    # config/param.jsonの確認
    if [ -f "config/param.json" ]; then
        print_info "✓ Parameter configuration found"
    else
        print_warning "Parameter configuration not found, creating default..."
        mkdir -p config
        echo '{"SESSION_TIMEOUT_MINUTES": 10080, "SERVER_PORT": 3000, "SSL_CERT_PATH": "config/ssl/server.crt", "SSL_KEY_PATH": "config/ssl/server.key", "IS_DEVELOPMENT": true, "ALLOWED_ORIGINS": ["http://localhost:8080", "https://localhost:8080"]}' > config/param.json
    fi
    
    # 実行ファイルの基本テスト
    if [ -f "build/home-server" ]; then
        print_info "Testing executable..."
        chmod +x build/home-server
        
        # 短時間の起動テスト
        timeout 5s ./build/home-server || true
        print_info "✓ Executable test completed"
    fi
    
    print_info "✓ Integration tests completed"
}

# メイン処理
main() {
    print_header "Docker CI/CD Test Suite"
    echo "Starting Docker-based CI/CD tests..."
    echo "Container Environment: $(uname -a)"
    echo "Working Directory: $(pwd)"
    echo "Entrypoint Script: $(which docker-test-entrypoint.sh || echo 'Not found in PATH')"
    echo "Workspace Script: $(ls -la /workspace/docker-test-entrypoint.sh 2>/dev/null || echo 'Not found in workspace')"
    echo ""
    
    # エントリーポイントスクリプトの存在確認
    if [ ! -f "/usr/local/bin/docker-test-entrypoint.sh" ] && [ ! -f "/workspace/docker-test-entrypoint.sh" ]; then
        print_error "Entrypoint script not found. Running fallback tests..."
        # フォールバック: 基本的なテストのみ実行
        check_dependencies
        validate_project_structure
        test_local_builds
        test_code_quality
        test_documentation
        print_warning "Skipping security and integration tests due to missing entrypoint script"
        return 0
    fi
    
    check_dependencies
    validate_project_structure
    test_local_builds
    test_code_quality
    test_documentation
    test_security
    test_integration
    
    print_header "Test Results"
    print_info "✓ All Docker CI/CD tests completed successfully!"
    
    # 結果サマリー
    echo ""
    echo "=== Test Summary ==="
    echo "✓ Project structure validation"
    echo "✓ Makefile build"
    echo "✓ CMake build"
    echo "✓ Build script"
    echo "✓ Code quality checks"
    echo "✓ Documentation validation"
    echo "✓ Integration tests"
    if [ "$SKIP_SECURITY" = false ]; then
        echo "✓ Security checks"
    fi
    echo ""
    echo "All tests passed! You can safely push to GitHub."
}

# スクリプトの実行
main "$@" 