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
BUILD_IMAGE=false
CLEAN_UP=true
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --quick)
            TEST_TYPE="quick"
            shift
            ;;
        --build)
            BUILD_IMAGE=true
            shift
            ;;
        --no-cleanup)
            CLEAN_UP=false
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --quick         Quick test (skip security scans)"
            echo "  --build         Build Docker image before testing"
            echo "  --no-cleanup    Don't clean up containers after test"
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

# Dockerの確認
check_docker() {
    if ! command -v docker &> /dev/null; then
        print_error "Docker is not installed or not in PATH"
        exit 1
    fi
    
    if ! docker info &> /dev/null; then
        print_error "Docker daemon is not running or you don't have permission"
        print_info "Try: sudo systemctl start docker"
        print_info "Or add your user to docker group: sudo usermod -aG docker $USER"
        exit 1
    fi
    
    print_info "Docker is available"
}

# Dockerイメージのビルド
build_test_image() {
    print_header "Building Test Docker Image"
    
    local image_name="home-server-ci-test"
    local dockerfile="docker/Dockerfile.test"
    
    if [ ! -f "$dockerfile" ]; then
        print_error "Test Dockerfile not found: $dockerfile"
        exit 1
    fi
    
    print_info "Building Docker image: $image_name"
    
    local build_args=""
    if [ "$VERBOSE" = true ]; then
        build_args="$build_args --progress=plain"
    fi
    
    docker build -f "$dockerfile" -t "$image_name" . $build_args
    
    if [ $? -eq 0 ]; then
        print_info "✓ Docker image built successfully"
    else
        print_error "Failed to build Docker image"
        exit 1
    fi
}

# テストの実行
run_tests() {
    print_header "Running Docker CI/CD Tests"
    
    local image_name="home-server-ci-test"
    local container_name="home-server-ci-test-$(date +%s)"
    local test_args=""
    
    # テストタイプに応じた引数を設定
    if [ "$TEST_TYPE" = "quick" ]; then
        test_args="--quick"
    fi
    
    if [ "$VERBOSE" = true ]; then
        test_args="$test_args --verbose"
    fi
    
    print_info "Starting test container: $container_name"
    print_info "Test arguments: $test_args"
    
    # コンテナを実行
    docker run --rm \
        --name "$container_name" \
        -v "$(pwd):/workspace" \
        -w /workspace \
        --entrypoint /usr/local/bin/docker-test-entrypoint.sh \
        "$image_name" \
        $test_args
    
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        print_info "✓ All tests passed!"
    else
        print_error "Tests failed with exit code: $exit_code"
        
        # コンテナのログを表示
        if [ "$VERBOSE" = true ]; then
            print_info "Container logs:"
            docker logs "$container_name" 2>/dev/null || true
        fi
    fi
    
    return $exit_code
}

# クリーンアップ
cleanup() {
    if [ "$CLEAN_UP" = true ]; then
        print_header "Cleaning Up"
        
        # テストコンテナの停止（念のため）
        docker stop home-server-ci-test-* 2>/dev/null || true
        
        # テストイメージの削除（オプション）
        read -p "Remove test Docker image? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            docker rmi home-server-ci-test 2>/dev/null || true
            print_info "Test image removed"
        fi
    fi
}

# メイン処理
main() {
    print_header "Docker CI/CD Test Runner"
    echo "Test Type: $TEST_TYPE"
    echo "Build Image: $BUILD_IMAGE"
    echo "Clean Up: $CLEAN_UP"
    echo "Verbose: $VERBOSE"
    echo ""
    
    # Dockerの確認
    check_docker
    
    # イメージのビルド（必要に応じて）
    if [ "$BUILD_IMAGE" = true ]; then
        build_test_image
    else
        # イメージが存在するかチェック
        if ! docker image inspect home-server-ci-test &> /dev/null; then
            print_warning "Test image not found, building..."
            build_test_image
        else
            print_info "Using existing test image"
        fi
    fi
    
    # テストの実行
    local test_exit_code=0
    run_tests || test_exit_code=$?
    
    # クリーンアップ
    cleanup
    
    # 終了
    if [ $test_exit_code -eq 0 ]; then
        print_header "Success"
        print_info "All Docker CI/CD tests completed successfully!"
        print_info "You can safely push to GitHub."
    else
        print_header "Failure"
        print_error "Docker CI/CD tests failed!"
        print_info "Please fix the issues before pushing to GitHub."
    fi
    
    exit $test_exit_code
}

# スクリプトの実行
main "$@" 