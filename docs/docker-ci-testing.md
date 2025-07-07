# Docker環境でのCI/CDテスト

このドキュメントでは、Docker環境でローカルCI/CDテストを実行する方法を説明します。

## 概要

Docker環境でのCI/CDテストにより、以下のメリットがあります：

- **本番環境の保護**: ホスト環境を汚さない
- **一貫した環境**: 常に同じテスト環境で実行
- **依存関係の分離**: テストに必要なツールを独立して管理
- **再現性**: 同じ結果を確実に得られる

## アーキテクチャ

```
ホスト環境 (Ubuntu 24.04)
├── Docker Engine
└── テストコンテナ (Ubuntu 24.04)
    ├── ビルドツール (GCC, CMake, Make)
    ├── 依存関係 (Crow, OpenSSL)
    ├── テストツール (cppcheck)
    └── プロジェクトコード (マウント)
```

## 必要なツール

### ホスト環境
- `docker` (Docker Engine)
- `make` (Makefileターゲット実行用)

### コンテナ環境（自動インストール）
- `build-essential`
- `cmake`
- `git`
- `libssl-dev`
- `cppcheck`
- `Crow framework`

## 使用方法

### 1. 基本的なDockerテスト

```bash
# 全テストを実行（推奨）
make test-ci-local

# または直接スクリプトを実行
./scripts/docker-ci-runner.sh
```

### 2. クイックテスト（セキュリティスキャンをスキップ）

```bash
# 高速テスト
make test-ci-local-quick

# または
./scripts/docker-ci-runner.sh --quick
```

### 3. イメージの再ビルド

```bash
# テストイメージを再ビルドしてからテスト
make test-ci-local-build

# または
./scripts/docker-ci-runner.sh --build
```

### 4. 詳細ログ付きテスト

```bash
# 詳細なログを表示
make test-ci-local-verbose

# または
./scripts/docker-ci-runner.sh --verbose
```

### 5. ホスト環境でのテスト（非推奨）

```bash
# 本番環境を汚す可能性があるため非推奨
make test-ci-host
```

## スクリプトオプション

`scripts/docker-ci-runner.sh`のオプション：

```bash
./scripts/docker-ci-runner.sh [OPTIONS]

Options:
  --quick         Quick test (skip security scans)
  --build         Build Docker image before testing
  --no-cleanup    Don't clean up containers after test
  --verbose       Enable verbose output
  --help          Show this help message
```

## Dockerイメージの詳細

### ベースイメージ
- **Ubuntu 24.04**: 最新のLTS版を使用

### インストールされるツール
```dockerfile
# ビルドツール
build-essential
cmake
git
libssl-dev
pkg-config

# 開発ツール
libboost-all-dev
libasio-dev
cppcheck

# コンパイラ
gcc-13 g++-13 (利用可能な場合)
```

### 環境変数
```dockerfile
ENV DEBIAN_FRONTEND=noninteractive
ENV CMAKE_BUILD_TYPE=Release
ENV CXX_STANDARD=23
ENV WORKSPACE=/workspace
```

## テスト内容

### 1. 依存関係チェック
- 必要なツールの存在確認
- バージョン確認

### 2. プロジェクト構造検証
- 必要なファイルの存在確認
- ディレクトリ構造の確認

### 3. ビルドテスト
- **Makefileビルド**: `server_systemd.out`の生成
- **CMakeビルド**: `home-server`の生成
- **ビルドスクリプト**: `scripts/build.sh`のテスト

### 4. コード品質チェック
- cppcheckによる静的解析
- コードフォーマットチェック

### 5. ドキュメントチェック
- README.mdの構造確認
- HTMLファイルの構文チェック

### 6. セキュリティチェック（オプション）
- 基本的なセキュリティチェック
- ハードコードされたパスワードの確認

### 7. 統合テスト
- 実行ファイルの基本テスト
- テスト用設定ファイルの確認

## ファイル構造

```
docker/
├── Dockerfile          # 本番用Dockerfile
├── Dockerfile.test     # テスト用Dockerfile
└── Dockerfile.dev      # 開発用Dockerfile

scripts/
├── docker-ci-runner.sh        # Dockerテスト実行スクリプト
├── docker-test-entrypoint.sh  # コンテナ内エントリーポイント
├── test-ci-local.sh           # ホスト環境用テストスクリプト
└── build.sh                   # ビルドスクリプト
```

## トラブルシューティング

### よくある問題

#### 1. Dockerがインストールされていない
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y docker.io
sudo systemctl start docker
sudo systemctl enable docker

# ユーザーをdockerグループに追加
sudo usermod -aG docker $USER
# ログアウトして再ログイン
```

#### 2. Dockerデーモンが起動していない
```bash
sudo systemctl start docker
sudo systemctl status docker
```

#### 3. 権限エラー
```bash
# ユーザーをdockerグループに追加
sudo usermod -aG docker $USER

# または一時的にsudoを使用
sudo ./scripts/docker-ci-runner.sh
```

#### 4. イメージビルドエラー
```bash
# 詳細ログでビルド
./scripts/docker-ci-runner.sh --build --verbose

# 手動でビルド
docker build -f docker/Dockerfile.test -t home-server-ci-test .
```

#### 5. コンテナ実行エラー
```bash
# コンテナのログを確認
docker logs <container-name>

# コンテナ内で直接実行
docker run -it --rm -v $(pwd):/workspace -w /workspace home-server-ci-test bash
```

### ログの確認

テスト実行時のログは以下の方法で確認できます：

- **Dockerログ**: `docker logs <container-name>`
- **詳細ログ**: `--verbose`オプションを使用
- **コンテナ内ログ**: コンテナ内の`cppcheck-result.txt`

## パフォーマンス最適化

### 1. イメージキャッシュの活用
```bash
# イメージを再利用
./scripts/docker-ci-runner.sh  # 初回のみビルド

# 強制再ビルド
./scripts/docker-ci-runner.sh --build
```

### 2. ボリュームマウントの最適化
```bash
# 現在のディレクトリをマウント
-v "$(pwd):/workspace"
```

### 3. 並列実行（将来の拡張）
```bash
# 複数のテストを並列実行（実装予定）
make test-ci-parallel
```

## ベストプラクティス

### 1. 開発ワークフロー
```bash
# 1. コード変更
git add .
git commit -m "Your changes"

# 2. Dockerテスト（推奨）
make test-ci-local-quick

# 3. 問題がなければpush
git push origin main
```

### 2. 定期的な完全テスト
```bash
# 週1回程度、完全テストを実行
make test-ci-local
```

### 3. プルリクエスト前のテスト
```bash
# プルリクエストを作成する前に
make test-ci-local-verbose
```

### 4. クリーンアップ
```bash
# 不要なイメージを削除
docker image prune -f

# 不要なコンテナを削除
docker container prune -f
```

## セキュリティ考慮事項

### 1. コンテナの分離
- テストは完全に分離されたコンテナ内で実行
- ホスト環境への影響なし

### 2. 権限の最小化
- テストコンテナは必要最小限の権限で実行
- ホストファイルシステムは読み取り専用でマウント

### 3. ネットワーク分離
- テストコンテナは必要に応じてネットワークアクセスを制限

## 参考リンク

- [Docker公式ドキュメント](https://docs.docker.com/)
- [Dockerfile ベストプラクティス](https://docs.docker.com/develop/dev-best-practices/)
- [cppcheck公式ドキュメント](https://cppcheck.sourceforge.io/)
- [CMake公式ドキュメント](https://cmake.org/documentation/) 