# HOME-SERVER

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.25+-green.svg)](https://cmake.org/)
[![Docker](https://img.shields.io/badge/Docker-Ready-blue.svg)](https://www.docker.com/)

HOME-SERVERは、画像ビューアー、メモ管理、ユーザー管理機能を統合したWebベースのホームサーバーシステムです。セッションIDベースの認証システムとモダンなUIデザインを採用しています。

## 📋 目次

- [主な機能](#主な機能)
- [デザイン特徴](#デザイン特徴)
- [セットアップ](#セットアップ)
- [使用方法](#使用方法)
- [プロジェクト構造](#プロジェクト構造)
- [API エンドポイント](#api-エンドポイント)
- [権限システム](#権限システム)
- [セキュリティ機能](#セキュリティ機能)
- [トラブルシューティング](#トラブルシューティング)
- [技術仕様](#技術仕様)
- [注意事項](#注意事項)
- [ライセンス](#ライセンス)
- [貢献](#貢献)
- [セキュリティ](#セキュリティ)

## 主な機能

### 🔐 認証システム
- **セッションIDベース認証**: 安全なセッション管理
- **自動入力対応**: Chromeの自動入力機能に対応
- **権限管理**: 管理者と一般ユーザーの権限分離
- **セッション期限**: 30分間の自動セッション管理

### 🖼️ 画像ビューアー (Viewer)
- 画像ファイルの閲覧・管理
- メタデータ（作成者、タグ）の編集
- 検索機能とフィルタリング
- レスポンシブデザイン対応
- タッチジェスチャー対応

### 📝 メモ管理 (Memo)
- メモの作成・編集・管理
- リアルタイム保存
- シンプルで直感的なインターフェース
- モバイル対応

### 👥 ユーザー管理
- ユーザーの登録・削除
- 権限の昇格・降格
- 管理者権限による管理機能

## デザイン特徴

### 🎨 モダンなUI/UX
- **統一されたデザイン言語**: viewer、memo、indexページで一貫したデザイン
- **ガラスモーフィズム効果**: 半透明背景とブラー効果
- **グラデーション背景**: 美しいグラデーションカラーパレット
- **レスポンシブデザイン**: デスクトップ、タブレット、モバイル対応

### 📱 ハンバーガーメニュー
- ユーザー管理とログアウト機能をハンバーガーメニューに配置
- モバイルフレンドリーなナビゲーション
- 外側クリックで自動的に閉じる機能

## 🚀 クイックスタート

### Docker での実行（推奨）

```bash
# リポジトリのクローン
git clone https://github.com/clever-elsie/home-server.git
cd home-server

# Docker Compose での起動
cd docker
docker-compose up --build

# ブラウザで http://localhost:8080 にアクセス
```

### ローカルビルド

```bash
# 依存関係のインストール
sudo apt install build-essential cmake libssl-dev pkg-config

# ビルドと実行
mkdir build && cd build
cmake ..
make
./home-server
```

## 📚 セットアップ

詳細なセットアップ手順は [docs/SETUP.md](docs/SETUP.md) を参照してください。

### 前提条件
- C++23対応のコンパイラ (GCC 13.0以上)
- OpenSSLライブラリ
- systemd (Linux)
- nginx または Apache

### 1. 初回起動

初回起動時は、任意のユーザー名とパスワードで管理者アカウントを作成できます。
システム起動後、ブラウザでアクセスして初回ユーザー登録を行ってください。

### 2. ビルド方法

#### 方法1: CMake + ローカルビルド
```bash
# 依存関係のインストール
sudo apt install build-essential cmake libssl-dev pkg-config

# C++23対応のためGCC 13+をインストール（Ubuntu 22.04以降）
sudo apt install gcc-13 g++-13
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 130 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-13

# ビルド
./scripts/build.sh

# デバッグビルド
./scripts/build.sh --debug

# インストール付きビルド
./scripts/build.sh --install
```

#### 方法2: Docker環境
```bash
# 本番環境での実行
docker-compose up --build

# 開発環境での実行
docker-compose --profile dev up --build
```

### 3. ドメイン設定

システムは起動時に `config/domain` ファイルから許可されたオリジンを読み込みます。

#### ドメイン設定ファイルの編集
```bash
# 設定ファイルを編集
nano config/domain

# 例：本番環境での設定
https://192.168.10.108:443
https://your-domain.com
https://www.your-domain.com

# 開発環境での設定
http://localhost:3000
http://localhost:8080
https://localhost:3000
https://localhost:8080
```

#### 設定ファイルの形式
- 1行ごとに許可するorigin URLを記述
- コメント行は `#` で始まる
- 空行は無視される
- 設定ファイルが存在しない場合は、デフォルトの開発環境用設定が使用される

### 4. サーバー起動

#### 開発環境
```bash
# ローカルビルドの場合
./build/home-server

# Docker開発環境の場合
docker-compose --profile dev run --rm home-server-dev
```

#### 本番環境 (systemd)
```bash
sudo systemctl start home-server
sudo systemctl enable home-server
```

### 5. サービス管理

```bash
# サービス状態確認
make see

# サービス監視
make watch

# サービス再起動
make reload
```

## 使用方法

### 初回ユーザー登録

1. ブラウザで `https://your-domain.com/` にアクセス
2. 任意のユーザー名とパスワードで管理者アカウントを作成
3. 初回ユーザーは自動的に管理者権限を取得

### ログイン

1. ブラウザで `https://your-domain.com/` にアクセス
2. 登録済みのユーザー名とパスワードを入力してログイン
3. セッションIDが自動的に保存され、以降のリクエストで使用されます

### 機能利用

#### ホーム画面
- **Viewer**: 画像ビューアーにアクセス
- **Memo**: メモ管理にアクセス
- **ハンバーガーメニュー**: ユーザー管理とログアウト

#### 画像ビューアー
- 画像の閲覧とナビゲーション
- メタデータ（作成者、タグ）の編集
- 検索機能による画像検索
- 同じ作成者の作品表示

#### メモ管理
- 新規メモの作成
- 既存メモの編集・削除
- リアルタイム保存

### ログアウト

- ハンバーガーメニューから「ログアウト」をクリック
- または、30分間操作がないと自動的にセッションが期限切れになります

## プロジェクト構造

```
HOME-SERVER/
├── src/                           # ソースコード
│   ├── server/                    # バックエンド (C++)
│   │   ├── main.cpp              # メインサーバーファイル
│   │   ├── auth.hpp              # 認証システム
│   │   ├── middleware.hpp        # 認証ミドルウェア
│   │   ├── user_manager.hpp      # ユーザー管理システム
│   │   ├── user_api.hpp          # ユーザー管理API
│   │   ├── viewer.hpp            # 画像ビューアー機能
│   │   ├── memo.hpp              # メモ管理機能
│   │   ├── config.hpp            # 設定ファイル
│   │   └── headers.hpp           # 共通ヘッダー
│   └── frontend/                  # フロントエンド
│       ├── viewer/
│       │   ├── style.css         # デスクトップ用スタイル
│       │   ├── mobile.css        # モバイル用スタイル
│       │   └── src.js            # ビューアー機能
│       └── memo/
│           ├── style.css         # デスクトップ用スタイル
│           ├── mobile.css        # モバイル用スタイル
│           └── script.js         # メモ機能
├── web/                           # フロントエンドファイル
│   ├── index.html                 # メインページ（ログイン・ホーム）
│   ├── viewer.html                # 画像ビューアーページ
│   ├── memo.html                  # メモ管理ページ
│   └── user_register.html         # ユーザー管理ページ
├── config/                        # 設定ファイル
│   └── users.json.example         # ユーザー設定テンプレート
├── docker/                        # Docker関連ファイル
│   ├── Dockerfile                 # 本番用Dockerfile
│   ├── Dockerfile.dev             # 開発用Dockerfile
│   └── docker-compose.yml         # Docker Compose設定
├── docs/                          # ドキュメント
│   └── SETUP.md                   # セットアップガイド
├── scripts/                       # ビルド・デプロイスクリプト
├── data/                          # データディレクトリ
├── memo/                          # メモデータ
├── .github/                       # GitHub設定
│   ├── workflows/                 # CI/CD設定
│   └── ISSUE_TEMPLATE/            # Issueテンプレート
├── README.md                      # プロジェクト概要
├── CONTRIBUTING.md                # 貢献ガイドライン
├── SECURITY.md                    # セキュリティポリシー
├── LICENSE                        # ライセンス
├── CMakeLists.txt                 # CMake設定
├── Makefile                       # ビルド設定
└── .gitignore                     # Git除外設定
```

## API エンドポイント

### 認証関連
- `POST /req/auth/login` - ログイン（username, password）
- `POST /req/auth/logout` - ログアウト（session_id）
- `POST /req/auth/check` - 認証状態確認（session_id）

### ユーザー管理関連
- `POST /req/user/register` - ユーザー登録（username, password, role, created_by）
- `POST /req/user/delete` - ユーザー削除（username, deleted_by）
- `POST /req/user/promote` - ユーザー昇格（username, promoted_by）
- `POST /req/user/demote` - ユーザー降格（username, demoted_by）
- `GET /req/user/list` - ユーザー一覧取得
- `GET /req/user/check_first` - 初回ユーザー確認
- `POST /req/user/permissions` - ユーザー権限確認（username）

### 画像ビューアー関連
- `GET /req/viewer/*` - 画像ファイル取得
- `POST /req/viewer/metadata` - メタデータ更新
- `GET /req/viewer/search` - 画像検索

### メモ管理関連
- `GET /req/memo/list` - メモ一覧取得
- `POST /req/memo/create` - メモ作成
- `PUT /req/memo/update` - メモ更新
- `DELETE /req/memo/delete` - メモ削除

## 権限システム

### 管理者権限
- 全機能にアクセス可能
- ユーザーの登録・削除・権限変更が可能
- システム全体の管理が可能

### 一般ユーザー権限
- 基本機能（ビューアー、メモ）にアクセス可能
- ユーザー管理機能にはアクセス不可

### 初回起動時
- 初回起動時は誰でも管理者アカウントを作成可能
- 初回ユーザーは自動的に管理者権限を取得

## セキュリティ機能

- **ID/パスワード認証**: ユーザー名とパスワードの組み合わせによる認証
- **パスワードハッシュ化**: SHA-256によるパスワードハッシュ化
- **セッション期限**: 30分間操作がないと自動的にセッションが無効化
- **セッションID**: ランダムな16進数文字列で生成
- **HTTPS対応**: SSL/TLS暗号化による通信
- **CORS設定**: 許可されたオリジンのみアクセス可能
  - 開発環境: localhost系のオリジンのみ許可
  - 本番環境: 設定されたドメインのみ許可
- **セキュリティヘッダー**: XSS、クリックジャッキング、MIME型スニッフィング対策
- **認証ミドルウェア**: すべてのAPIエンドポイントで認証チェック
- **権限管理**: 管理者と一般ユーザーの権限分離
- **ユーザー管理**: 安全なユーザー登録・削除・権限変更機能

## トラブルシューティング

### 認証エラーが発生する場合
1. ユーザー名とパスワードが正しいか確認
2. セッションIDが期限切れになっていないか確認
3. ブラウザのlocalStorageが有効になっているか確認
4. ユーザーが正しく登録されているか確認

### ユーザー管理エラーが発生する場合
1. 管理者権限でログインしているか確認
2. 対象ユーザーが存在するか確認
3. 権限操作の対象が自分自身でないか確認

### コンパイルエラーが発生する場合
1. 必要なライブラリ（OpenSSL）がインストールされているか確認
2. C++20対応のコンパイラを使用しているか確認

### デザインが正しく表示されない場合
1. ブラウザがCSS GridとFlexboxをサポートしているか確認
2. JavaScriptが有効になっているか確認
3. フォント（Inter）が正しく読み込まれているか確認

## 技術仕様

### バックエンド
- **言語**: C++20
- **Webフレームワーク**: カスタムHTTPサーバー
- **認証**: セッションIDベース
- **データ保存**: JSONファイル
- **暗号化**: OpenSSL

### フロントエンド
- **HTML5**: セマンティックマークアップ
- **CSS3**: Grid、Flexbox、カスタムプロパティ
- **JavaScript**: ES6+、Fetch API
- **フォント**: Inter (Google Fonts)
- **アイコン**: Font Awesome 6.0

### レスポンシブデザイン
- **デスクトップ**: 1200px以上
- **タブレット**: 768px - 1199px
- **モバイル**: 767px以下

## 注意事項

- 本番環境では、HTTPS環境での使用を強く推奨します
- セッションIDはブラウザのlocalStorageに保存されるため、セキュリティに注意してください
- 定期的にパスワードを変更することを推奨します
- 管理者アカウントは適切に管理し、不要なユーザーは削除してください
- ユーザー情報は`users.json`ファイルに保存されるため、ファイルのバックアップを定期的に行ってください
- 画像ファイルは適切なディレクトリ構造で管理してください

## ライセンス

このプロジェクトはMITライセンスの下で公開されています。詳細は [LICENSE](LICENSE) ファイルを参照してください。

## 貢献

このプロジェクトへの貢献を歓迎します！

### 貢献の方法

1. **Issue の作成**: バグ報告や機能要望は [GitHub Issues](https://github.com/clever-elsie/archive/issues) でお知らせください
2. **Pull Request**: 機能追加やバグ修正のプルリクエストも歓迎します
3. **ドキュメント**: ドキュメントの改善や翻訳も歓迎します

### 開発環境のセットアップ

```bash
# リポジトリのクローン
git clone https://github.com/clever-elsie/archive.git
cd archive

# 開発環境での実行
docker-compose --profile dev up --build
```

### コーディング規約

- C++23標準に準拠
- 適切なコメントとドキュメント
- エラーハンドリングの実装
- セキュリティを考慮した実装

## セキュリティ
検討，改善の余地があると思われます

### 脆弱性の報告

セキュリティ上の問題を発見した場合は、直接メールで報告してください：
- **メール**: elsie.c13v3r@gmail.com
- **PGP Key**: [公開鍵へのリンク]

### セキュリティポリシー

- セキュリティ関連のIssueは非公開で処理します
- このリポジトリの内容はローカルネットワークで使用することを想定しています．
- 公開ネットワーク上ので使用における重大なセキュリティホールが発見されても特別措置は取らず通常の開発ペースを維持します．

## サポート

- **Issues**: [GitHub Issues](https://github.com/clever-elsie/archive/issues)
- **Discussions**: [GitHub Discussions](https://github.com/clever-elsie/archive/discussions)
- **Wiki**: [プロジェクトWiki](https://github.com/clever-elsie/archive/wiki) 