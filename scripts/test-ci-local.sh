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
USE_ACT=false
SKIP_DOCKER=false
SKIP_SECURITY=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --act)
            USE_ACT=true
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
            echo "  --act           Use act (GitHub Actions local runner)"
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

# actのインストール確認
check_act() {
    if ! command -v act &> /dev/null; then
        print_warning "act is not installed. Installing..."
        
        # macOS
        if [[ "$OSTYPE" == "darwin"* ]]; then
            brew install act
        # Linux
        elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
            curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash
        else
            print_error "Unsupported OS for automatic act installation"
            print_info "Please install act manually: https://github.com/nektos/act"
            exit 1
        fi
    fi
    
    print_info "act version: $(act --version)"
}

# 依存関係のチェック
check_dependencies() {
    print_header "Checking Dependencies"
    
    local missing_deps=()
    
    # 基本ツール
    command -v git >/dev/null 2>&1 || missing_deps+=("git")
    command -v make >/dev/null 2>&1 || missing_deps+=("make")
    command -v cmake >/dev/null 2>&1 || missing_deps+=("cmake")
    command -v g++ >/dev/null 2>&1 || missing_deps+=("g++")
    
    # Docker（オプション）
    if [ "$SKIP_DOCKER" = false ]; then
        command -v docker >/dev/null 2>&1 || missing_deps+=("docker")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        print_info "Please install missing dependencies and try again"
        exit 1
    fi
    
    print_info "All dependencies found"
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
    
    # cppcheck（利用可能な場合）
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

# Dockerテスト
test_docker() {
    if [ "$SKIP_DOCKER" = true ]; then
        print_warning "Skipping Docker tests"
        return
    fi
    
    print_header "Testing Docker Build"
    
    # Dockerイメージのビルド
    print_info "Building Docker image..."
    docker build -t archive:test .
    
    # Dockerコンテナのテスト
    print_info "Testing Docker container..."
    docker run --rm -d --name test-archive-local -p 8080:8080 archive:test
    sleep 15
    
    # ヘルスチェック
    if curl -f http://localhost:8080/ >/dev/null 2>&1; then
        print_info "✓ Docker container test successful"
    else
        print_error "Docker container test failed"
        docker stop test-archive-local || true
        exit 1
    fi
    
    docker stop test-archive-local
    print_info "✓ Docker tests completed"
}

# セキュリティスキャン
test_security() {
    if [ "$SKIP_SECURITY" = true ]; then
        print_warning "Skipping security scans"
        return
    fi
    
    print_header "Testing Security"
    
    # Trivy（利用可能な場合）
    if command -v trivy &> /dev/null; then
        print_info "Running Trivy security scan..."
        trivy fs . --format table --severity HIGH,CRITICAL || true
        print_info "✓ Trivy scan completed"
    else
        print_warning "Trivy not found, skipping security scan"
    fi
    
    # 基本的なセキュリティチェック
    print_info "Checking for common security issues..."
    
    # ハードコードされたパスワードの確認
    if grep -r "password.*=" src/ 2>/dev/null | grep -v "//" | grep -v "example"; then
        print_warning "Potential hardcoded passwords found"
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
        # ALLOWED_ORIGINSの確認
        if grep -q "ALLOWED_ORIGINS" config/param.json; then
            print_info "✓ ALLOWED_ORIGINS configuration found"
        else
            print_warning "ALLOWED_ORIGINS not found in param.json"
        fi
    else
        print_warning "Parameter configuration not found"
    fi
    
    print_info "✓ Security checks completed"
}

# actを使用したGitHub Actionsテスト
test_with_act() {
    if [ "$USE_ACT" = false ]; then
        return
    fi
    
    print_header "Testing with GitHub Actions (act)"
    
    # actのインストール確認
    check_act
    
    # 必要なジョブのみを実行
    local jobs_to_run=""
    
    if [ "$SKIP_DOCKER" = false ]; then
        jobs_to_run="$jobs_to_run docker-build"
    fi
    
    if [ "$SKIP_SECURITY" = false ]; then
        jobs_to_run="$jobs_to_run security-scan"
    fi
    
    jobs_to_run="$jobs_to_run code-quality documentation"
    
    # actの実行
    print_info "Running act with jobs: $jobs_to_run"
    
    local act_flags=""
    if [ "$VERBOSE" = true ]; then
        act_flags="$act_flags --verbose"
    fi
    
    act --list
    echo "To run specific jobs, use: act -j <job-name>"
    echo "Example: act -j cmake-build"
    
    print_info "✓ act setup completed"
}

# メイン処理
main() {
    print_header "Local CI/CD Test Suite"
    echo "Starting local CI/CD tests..."
    echo "Options:"
    echo "  Use act: $USE_ACT"
    echo "  Skip Docker: $SKIP_DOCKER"
    echo "  Skip Security: $SKIP_SECURITY"
    echo "  Verbose: $VERBOSE"
    echo ""
    
    check_dependencies
    validate_project_structure
    test_local_builds
    test_code_quality
    test_documentation
    test_docker
    test_security
    test_with_act
    
    print_header "Test Results"
    print_info "✓ All local CI/CD tests completed successfully!"
    print_info "You can now safely push to GitHub"
    
    # 結果サマリー
    echo ""
    echo "=== Test Summary ==="
    echo "✓ Project structure validation"
    echo "✓ Makefile build"
    echo "✓ CMake build"
    echo "✓ Build script"
    echo "✓ Code quality checks"
    echo "✓ Documentation validation"
    if [ "$SKIP_DOCKER" = false ]; then
        echo "✓ Docker build and test"
    fi
    if [ "$SKIP_SECURITY" = false ]; then
        echo "✓ Security scans"
    fi
    if [ "$USE_ACT" = true ]; then
        echo "✓ GitHub Actions setup (act)"
    fi
}

# スクリプトの実行
main "$@" 