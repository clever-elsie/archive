# ローカルCI/CDテスト

このドキュメントでは、GitHubにpushする前にローカルでCI/CDテストを実行する方法を説明します。

## 概要

ローカルCI/CDテストにより、以下のメリットがあります：

- **早期エラー発見**: push前に問題を特定
- **開発効率向上**: 手動テストの負担軽減
- **信頼性向上**: 確実にテストが通ってからpush

## 必要なツール

### 基本ツール
- `git`
- `make`
- `cmake`
- `g++` (GCC 13推奨)

### オプションツール
- `docker` (Dockerテスト用)
- `act` (GitHub Actionsローカル実行用)
- `cppcheck` (コード品質チェック用)
- `trivy` (セキュリティスキャン用)

## 使用方法

### 1. 基本的なローカルテスト

```bash
# 全テストを実行
make test-ci-local

# または直接スクリプトを実行
./scripts/test-ci-local.sh
```

### 2. クイックテスト（Dockerとセキュリティスキャンをスキップ）

```bash
# 高速テスト
make test-ci-local-quick

# または
./scripts/test-ci-local.sh --skip-docker --skip-security
```

### 3. GitHub Actionsをローカルで実行（act使用）

```bash
# actを使用したテスト
make test-ci-act

# クイック版
make test-ci-act-quick
```

### 4. 個別テスト

```bash
# Makefileビルドのみ
make test-makefile

# CMakeビルドのみ
make test-cmake

# Dockerビルドのみ
make test-docker

# ドキュメントチェックのみ
make test-docs
```

## スクリプトオプション

`scripts/test-ci-local.sh`のオプション：

```bash
./scripts/test-ci-local.sh [OPTIONS]

Options:
  --act           Use act (GitHub Actions local runner)
  --skip-docker   Skip Docker-related tests
  --skip-security Skip security scans
  --verbose       Enable verbose output
  --help          Show this help message
```

## actのインストール

### Linux
```bash
curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash
```

### macOS
```bash
brew install act
```

### Windows
```bash
# Chocolatey
choco install act-cli

# Scoop
scoop install act
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

### 6. Dockerテスト（オプション）
- Dockerイメージのビルド
- コンテナの起動テスト
- ヘルスチェック

### 7. セキュリティスキャン（オプション）
- Trivyによる脆弱性スキャン
- 基本的なセキュリティチェック

## トラブルシューティング

### よくある問題

#### 1. 依存関係エラー
```bash
# 必要なパッケージをインストール
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev pkg-config
```

#### 2. Crowフレームワークエラー
```bash
# Crowフレームワークを手動でインストール
git clone https://github.com/CrowCpp/Crow.git
cd Crow
mkdir build && cd build
cmake .. -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF
make -j$(nproc)
sudo make install
```

#### 3. Dockerエラー
```bash
# Dockerサービスを開始
sudo systemctl start docker
sudo systemctl enable docker

# ユーザーをdockerグループに追加
sudo usermod -aG docker $USER
```

#### 4. actエラー
```bash
# actの設定を確認
act --list

# 特定のジョブのみ実行
act -j cmake-build
```

### ログの確認

テスト実行時のログは以下の場所で確認できます：

- **cppcheck結果**: `cppcheck-result.txt`
- **actログ**: コンソール出力
- **Dockerログ**: `docker logs <container-name>`

## ベストプラクティス

### 1. 開発ワークフロー
```bash
# 1. コード変更
git add .
git commit -m "Your changes"

# 2. ローカルテスト
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
make test-ci-act
```

## 設定ファイル

### `.actrc`
actの設定ファイルです。必要に応じて調整してください：

```bash
# 使用するDockerイメージ
-P ubuntu-latest=catthehacker/ubuntu:act-latest

# ポートバインディング
--bind

# 詳細出力
--verbose
```

## パフォーマンス最適化

### 1. キャッシュの活用
```bash
# Dockerビルドキャッシュを活用
docker build --cache-from archive:latest -t archive:test .
```

### 2. 並列実行
```bash
# 複数のテストを並列実行（可能な場合）
make test-makefile & make test-cmake & wait
```

### 3. 条件付きテスト
```bash
# 変更されたファイルのみテスト
git diff --name-only HEAD~1 | grep -E '\.(cpp|hpp)$' && make test-cmake
```

## 参考リンク

- [act公式ドキュメント](https://github.com/nektos/act)
- [GitHub Actions公式ドキュメント](https://docs.github.com/en/actions)
- [cppcheck公式ドキュメント](https://cppcheck.sourceforge.io/)
- [Trivy公式ドキュメント](https://aquasecurity.github.io/trivy/) 