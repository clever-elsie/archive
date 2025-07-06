#!/bin/bash

# ドメイン設定ファイル自動生成スクリプト
# このスクリプトはビルド時に実行され、config/domainファイルを生成します

set -e

CONFIG_DIR="config"
DOMAIN_FILE="$CONFIG_DIR/domain"
DOMAIN_EXAMPLE="$CONFIG_DIR/domain.example"

echo "Generating domain configuration file..."

# configディレクトリが存在しない場合は作成
if [ ! -d "$CONFIG_DIR" ]; then
    mkdir -p "$CONFIG_DIR"
    echo "Created config directory: $CONFIG_DIR"
fi

# ドメイン設定ファイルが存在しない場合は、exampleファイルからコピー
if [ ! -f "$DOMAIN_FILE" ]; then
    if [ -f "$DOMAIN_EXAMPLE" ]; then
        cp "$DOMAIN_EXAMPLE" "$DOMAIN_FILE"
        echo "Created $DOMAIN_FILE from $DOMAIN_EXAMPLE"
        echo "Please edit $DOMAIN_FILE to configure your allowed domains"
    else
        # exampleファイルも存在しない場合は、基本的な設定を作成
        cat > "$DOMAIN_FILE" << 'EOF'
# ドメイン設定ファイル
# 1行ごとに許可するorigin URLを記述してください
# コメント行は#で始まります

# 開発環境用
http://localhost:3000
http://localhost:8080
https://localhost:3000
https://localhost:8080
http://127.0.0.1:3000
http://127.0.0.1:8080
https://127.0.0.1:3000
https://127.0.0.1:8080

# 本番環境用（コメントアウトを解除して設定）
# https://your-domain.com
EOF
        echo "Created basic $DOMAIN_FILE"
        echo "Please edit $DOMAIN_FILE to configure your allowed domains"
    fi
else
    echo "Domain configuration file already exists: $DOMAIN_FILE"
fi

echo "Domain configuration generation completed." 