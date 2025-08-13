# HOME-SERVER セットアップガイド

## 前提条件

- **OS**: Linux (Ubuntu 22.04+ 推奨)
- **コンパイラ**: GCC 13.0以上 (C++23対応)
- **ライブラリ**: OpenSSL
- **Webサーバー**: nginx または Apache (リバースプロキシ用)

## インストール手順

### 1. 依存関係のインストール

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential libssl-dev nginx

# C++23対応のためGCC 13+をインストール（Ubuntu 22.04以降）
sudo apt install gcc-13 g++-13
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 130 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-13
```

#### CentOS/RHEL
```bash
sudo yum groupinstall "Development Tools"
sudo yum install openssl-devel nginx
```

### 2. プロジェクトのクローン
```bash
git clone https://github.com/your-username/home-server.git
cd home-server
```

### 3. 必要なディレクトリの作成
```bash
mkdir -p data
mkdir -p memo/buf
mkdir -p DL
```

### 4. サンプル設定ファイルのコピー
```bash
cp users.json.example users.json
```

### 5. ビルド方法

#### 方法1: CMake + ローカルビルド
```bash
# 必要な依存関係のインストール
sudo apt install cmake pkg-config

# ビルドスクリプトの実行
./scripts/build.sh

# デバッグビルドの場合
./scripts/build.sh --debug

# クリーンビルドの場合
./scripts/build.sh --clean

# インストール付きビルドの場合
./scripts/build.sh --install
```

### 6. 開発環境での起動
```bash
# ローカルビルドの場合
./build/home-server
```

### 7. 本番環境での設定

#### systemdサービスのインストール
```bash
sudo make install
```

#### nginx設定例
```nginx
server {
    listen 80;
    server_name your-domain.com;
    
    location / {
        proxy_pass http://localhost:3000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

## 初回設定

1. ブラウザで `http://localhost:3000` にアクセス
2. 初回ユーザーとして管理者アカウントを作成
3. ログイン後、各機能を利用可能

## トラブルシューティング

### コンパイルエラー
- GCC 13.0以上がインストールされているか確認（C++23対応）
- OpenSSLライブラリがインストールされているか確認
- `string.contains`エラーが出る場合は、コンパイラのバージョンを確認

### 起動エラー
- ポート3000が使用可能か確認
- 必要なディレクトリが作成されているか確認

### 権限エラー
- ファイルの読み書き権限を確認
- systemdサービスを使用している場合は適切な権限で実行されているか確認

## セキュリティ注意事項

- 本番環境では必ずHTTPSを使用
- ファイアウォールでポート3000への直接アクセスを制限
- 定期的にパスワードを変更
- 不要なユーザーアカウントは削除

## サポート

問題が発生した場合は、GitHubのIssuesページで報告してください。

## Makefile用パラメータのカスタマイズ

- `./scripts/build.sh` 実行時に `makefile.env` が自動生成されます（既存の場合は上書きされません）。
- サービス名やバイナリ名を変更したい場合は、`makefile.env` を編集してください。
  - 例:
    ```
    SERVICE=yourservicename
    OUT=yourbinary.out
    ```
- `makefile.env` は `.gitignore` に含まれており、リポジトリにはコミットされません。 

### Viewer の参照ディレクトリを変更する

`makefile.env` で `VIEWER_DIR` を定義すると、ビルド時にビューアのベースディレクトリを差し替えられます。

- 空文字列（未定義を含む）の場合: 従来どおり、カレントディレクトリからの `data` を使用
- 非空の場合: 指定された絶対パスをそのまま使用

```
# 例: 絶対パスを指定
VIEWER_DIR=/mnt/storage/pictures

# 既定（空）の場合は data を使用
# VIEWER_DIR=
```

上記を設定後、通常どおり `make` でビルド・起動してください。

注意:
- サーバーはファイルをバイナリで返すため、フロントエンドの取得処理には影響しません（ID とファイル名で解決）。
- `web/viewer.html` 上部に表示するパス整形の都合で、画面表示用のパスが新しいベースディレクトリに合わない場合があります（`src/frontend/viewer/src.js` の `remove_prefix()` が固定値のため）。表示のみの問題で、機能には影響しません。