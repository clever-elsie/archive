#!/bin/bash

set -e

# 色付き出力
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# 引数の確認
if [ $# -ne 1 ]; then
    echo "Usage: $0 <project_root>"
    exit 1
fi

PROJECT_ROOT="$1"
SSL_DIR="${PROJECT_ROOT}/config/ssl"

# 既存のSSL証明書をチェック
SSL_KEY_FILE=""
SSL_CERT_FILE=""

# /etc/nginx/ssl内の既存証明書を検索
if [ -d "/etc/nginx/ssl" ]; then
    FOUND_KEY=$(find /etc/nginx/ssl -name "*.key" 2>/dev/null | head -1)
    FOUND_CERT=$(find /etc/nginx/ssl -name "*.crt" 2>/dev/null | head -1)
    
    if [ -n "$FOUND_KEY" ] && [ -n "$FOUND_CERT" ]; then
        SSL_KEY_FILE="$FOUND_KEY"
        SSL_CERT_FILE="$FOUND_CERT"
        print_info "Found existing SSL certificates:"
        print_info "  Key: $SSL_KEY_FILE"
        print_info "  Cert: $SSL_CERT_FILE"
        print_info "Skipping SSL certificate generation"
        exit 0
    fi
fi

# 既存の証明書が見つからない場合は生成
print_warning "No existing SSL certificates found, generating new ones..."

# SSLディレクトリの作成
mkdir -p "$SSL_DIR"

# 自己署名証明書の生成
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
    -keyout "${SSL_DIR}/server.key" \
    -out "${SSL_DIR}/server.crt" \
    -subj "/C=JP/ST=Tokyo/L=Tokyo/O=HomeServer/CN=localhost"

# 権限の設定
chmod 600 "${SSL_DIR}/server.key"
chmod 644 "${SSL_DIR}/server.crt"

print_info "SSL certificate generated successfully:"
print_info "  Key: ${SSL_DIR}/server.key"
print_info "  Cert: ${SSL_DIR}/server.crt" 