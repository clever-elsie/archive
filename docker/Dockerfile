# マルチステージビルド
FROM ubuntu:24.04 AS builder

# メタデータ
LABEL maintainer="clever-elsie <elsie.c13v3r@gmail.com>"
LABEL description="HOME-SERVER - Web-based home server with image viewer and memo management"
LABEL version="1.0.0"
LABEL org.opencontainers.image.source="https://github.com/clever-elsie/archive"

# 環境変数
ENV DEBIAN_FRONTEND=noninteractive
ENV CMAKE_BUILD_TYPE=Release

# 必要なパッケージのインストール
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    pkg-config \
    wget \
    libboost-all-dev \
    python3 \
    libasio-dev \
    && apt-get install -y gcc-13 g++-13 2>/dev/null || echo "GCC 13 not available" \
    && rm -rf /var/lib/apt/lists/*

# GCC 13をデフォルトに設定（存在する場合のみ）
RUN echo "=== Checking GCC installation ===" && \
    which gcc || echo "gcc not found in PATH" && \
    ls -la /usr/bin/gcc* 2>/dev/null || echo "No gcc files found" && \
    dpkg -l | grep build-essential || echo "build-essential not installed" && \
    if [ -f /usr/bin/gcc-13 ]; then \
        echo "Setting GCC 13 as default"; \
        update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 130 \
        --slave /usr/bin/g++ g++ /usr/bin/g++-13 \
        --slave /usr/bin/gcov gcov /usr/bin/gcov-13; \
    elif [ -f /usr/bin/gcc-12 ]; then \
        echo "Setting GCC 12 as default"; \
        update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 120 \
        --slave /usr/bin/g++ g++ /usr/bin/g++-12 \
        --slave /usr/bin/gcov gcov /usr/bin/gcov-12; \
    elif [ -f /usr/bin/gcc ]; then \
        echo "Using default GCC"; \
        gcc --version; \
    else \
        echo "No GCC found, reinstalling build-essential"; \
        apt-get update && apt-get install -y build-essential && \
        gcc --version; \
    fi && \
    echo "=== End GCC check ==="

# 作業ディレクトリの設定
WORKDIR /app

# Crowフレームワークのクローンとインストール
RUN git clone https://github.com/CrowCpp/Crow.git && \
    cd Crow && \
    mkdir build && \
    cd build && \
    cmake .. -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF && \
    make -j$(nproc) && \
    make install && \
    make clean && \
    echo "=== Crow installation completed ==="

# ソースコードのコピー
COPY . .

# dataディレクトリの確認と作成
RUN if [ ! -d "data" ]; then mkdir -p data; fi

# memoディレクトリの作成
RUN mkdir -p memo/buf

# SSL証明書ディレクトリの作成と証明書の生成
RUN mkdir -p /etc/nginx/ssl && \
    openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
    -keyout /etc/nginx/ssl/s.key \
    -out /etc/nginx/ssl/s.crt \
    -subj "/C=JP/ST=Tokyo/L=Tokyo/O=HomeServer/CN=localhost" && \
    chmod 600 /etc/nginx/ssl/s.key && \
    chmod 644 /etc/nginx/ssl/s.crt

# ビルドディレクトリの作成とビルド
RUN mkdir -p build && cd build \
    && cmake .. \
    && make -j$(nproc)

# 実行環境
FROM ubuntu:24.04 AS runtime

# 必要なパッケージのインストール
RUN apt-get update && apt-get install -y \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# ユーザーの作成
RUN useradd -m -s /bin/bash homeserver

# 作業ディレクトリの設定
WORKDIR /app

# ビルドされた実行ファイルのコピー
COPY --from=builder /app/build/home-server /app/home-server

# 設定ファイルのコピー
COPY --from=builder /app/config/users.json.example /app/users.json

# 静的ファイルのコピー
COPY --from=builder /app/web/*.html /app/
COPY --from=builder /app/data /app/data
COPY --from=builder /app/memo /app/memo
COPY --from=builder /etc/nginx/ssl /etc/nginx/ssl

# 権限の設定
RUN chown -R homeserver:homeserver /app \
    && chmod +x /app/home-server \
    && chown -R homeserver:homeserver /etc/nginx/ssl \
    && chmod 700 /etc/nginx/ssl \
    && chmod 600 /etc/nginx/ssl/s.key \
    && chmod 644 /etc/nginx/ssl/s.crt

# ユーザーの切り替え
USER homeserver

# ポートの公開
EXPOSE 8080

# ヘルスチェック
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/ || exit 1

# 実行コマンド
CMD ["./home-server"] 