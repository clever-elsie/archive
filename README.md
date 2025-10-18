# HOME-SERVER

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.25+-green.svg)](https://cmake.org/)

archiveは、メディアビューアー、メモ管理、ユーザー管理機能を統合したWebベースのホームサーバーシステムです。

## 目次

- [主な機能](#主な機能)
- [セットアップ](#セットアップ)
- [使用方法](#使用方法)
- [プロジェクト構造](#プロジェクト構造)
- [権限システム](#権限システム)
- [トラブルシューティング](#トラブルシューティング)
- [技術仕様](#技術仕様)
- [注意事項](#注意事項)
- [ライセンス](#ライセンス)
- [開発者向け](#開発者向け)

## 主な機能

### 認証システム
- **JWTトークンベース認証**: 安全なトークン管理
- **権限管理**: 管理者と一般ユーザーの権限分離
- **トークン期限**: 設定可能な有効期限（デフォルト: 1週間）

### 画像ビューアー (Viewer)
- メディアファイルの閲覧・管理
- タグの編集
- 検索機能とフィルタリング
- レスポンシブデザイン対応

### メモ管理 (Memo)
- メモの作成・編集・管理
- 共有メモの作成・編集・管理

### ユーザー管理
- ユーザーの登録・削除
- 権限の昇格・降格
- 管理者権限による管理機能

## クイックスタート

### ローカルビルド

事前にnginxの設定を行う必要があります．

```bash
# 依存関係のインストール
sudo apt install build-essential cmake libssl-dev pkg-config
# Crowのインストール
git clone https://github.com/CrowCpp/Crow.git
mkdir build && cd build
cmake .. -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF
make install

# ビルドと実行
make install
```

## セットアップ

### 前提条件
- C++23対応のコンパイラ (GCC 13.0以上)
- OpenSSLライブラリ
- systemd (Linux)
- nginx または Apache

### 1. nginxの設定
```bash
sudo apt install nginx
cd /etc/nginx/sites-available
sudo vim default
```
設定ファイルに`config/nginx.default.example`を参考に必要な情報を追加．
```bash
sudo systemctl reload nginx
```

### 2. ビルド方法

```bash
# 依存関係のインストール
sudo apt install build-essential cmake libssl-dev pkg-config

# C++23対応のためGCC 13+をインストール（Ubuntu 22.04以降）
apt search '^g\+\+-[0-9]+$' # これで見つかる一番新しいバージョンを使う
sudo apt install gcc-14 g++-14
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 130 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-13

# Crowのインストール
git clone https://github.com/CrowCpp/Crow.git
mkdir build && cd build
cmake .. -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF
make install

# ビルドとsystemdへの登録と実行
# このコマンドは`config/param.json`の設定が終わってから行うこと
# このコマンドはsystemdへ登録する`.service`ファイルを配置し終わった後に行うこと
make install
```

### 3. 起動準備
`make install`は`makefile.env`に`SERVICE=service_name`で指定された名前のサービスとして起動します．  
`/etc/systemd/system/service_name.service`を必要とします．  
`.service`ファイルは`config/home-server.service.in`を参考に記述してください．

システムは起動時に `config/param.json` ファイルから設定パラメータを読み込みます。  
詳しくは`config/param.json.example`を参照．
#### JWT秘密鍵の生成方法
JWTの秘密鍵（`JWT_SECRET_KEY`）は十分な長さのランダムな文字列を使用してください。

Linux/macOSターミナルで安全なランダムキーを生成する例：
```sh
# 32バイトのランダムな英数字（Base64）
openssl rand -base64 32
# 64バイトの16進数文字列
openssl rand -hex 64
```
生成した文字列を `config/param.json` の `JWT_SECRET_KEY` に設定してください。


### 4. 初回起動

初回起動時は、任意のユーザー名とパスワードで管理者アカウントを作成できます。  
システム起動後、ブラウザでアクセスして初回ユーザー登録を行ってください。  
初回起動時に登録するユーザー以外のすべてのユーザーは管理者権限を持つユーザーが登録しなければ登録できません．  

データは`users.json`に保存されます．  
パスワードはソルトが付加されハッシュ化されるため，内部を閲覧されても直ちに問題にはなりません．  
nginxの設定で`web/`と`src/frontend/`にアクセスを限定しているため，nginxの設定が不適切な場合は流出する可能性があるので管理には注意が必要です．

### 5. サービス管理

```bash
# systemdに登録し起動
make install

# サービス状態確認
make see

# サービス監視
make watch

# サービス再起動
make reload

# サービス停止
make stop
```

## 使用方法

### 初回ユーザー登録

1. ブラウザで `https://your-domain.com/` にアクセス
2. 任意のユーザー名とパスワードで管理者アカウントを作成
3. 初回ユーザーは自動的に管理者権限を取得

ローカル用を想定しているのでドメインは直接IPアドレスを入力することになるだろう．  
サーバーマシンのIPアドレスはルーターの設定で固定することを推奨する．

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
- 画像/動画/音声/テキストの閲覧とナビゲーション
- タグの編集
- 検索機能による画像検索
- ディレクトリベースの検索

ディレクトリの移動はシンボリックリンクも許可されているので，セキュリティホールにならないように注意が必要．  
また，シンボリックリンクを再帰的に走査するため，無限にノードを生成する可能性があることにも注意．

#### メモ管理
- 新規メモの作成
- 既存メモの編集・削除
- ログに残らない共有メモ

### ログアウト
- ハンバーガーメニューから「ログアウト」をクリック
- 自動ログアウトの時間は`config/param.json`で分単位で指定する．

## プロジェクト構造

```
archive/
├── CMakeLists.txt
├── config
│   ├── home-server.service.in
│   ├── nginx.default.example
│   ├── param.json
│   └── param.json.example
├── includes
│   ├── app
│   │   ├── memo/
│   │   └── viewer/
│   ├── lib/
│   └── manager
│       ├── auth/
│       └── users/
├── LICENSE
├── Makefile
├── makefile.env
├── README.md
├── src
│   ├── frontend
│   │   ├── memo/
│   │   └── viewer
│   │       └── api/
│   └── server
│       ├── app
│       │   ├── memo/
│       │   └── viewer/
│       ├── main.cpp
│       └── manager
│           ├── auth/
│           └── users/
├── users.json
└── web
    ├── index.html
    ├── memo.html
    ├── user_register.html
    └── viewer.html
```

## 権限システム

### 管理者権限
- 全機能にアクセス可能
- ユーザーの登録・削除・権限変更が可能
- viewerのディレクトリの更新をサーバー側に指示することができる

### 一般ユーザー権限
- 基本機能（ビューアー、メモ）にアクセス可能
- ユーザー管理機能にはアクセス不可

### 初回起動時
- 初回起動時は誰でも管理者アカウントを作成可能
- 初回ユーザーは自動的に管理者権限を取得

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
2. C++23対応のコンパイラを使用しているか確認

### デザインが正しく表示されない場合
1. ブラウザがCSS GridとFlexboxをサポートしているか確認
2. JavaScriptが有効になっているか確認
3. フォント（Inter）が正しく読み込まれているか確認

## 技術仕様

### バックエンド
- **言語**: C++23
- **Webフレームワーク**: CrowCpp/nginx
- **認証**: JWTトークンベース
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
- セッションIDはブラウザのlocalStorageに保存されるため、セキュリティに注意してください
- 定期的にパスワードを変更することを推奨します
- 管理者アカウントは適切に管理し、不要なユーザーは削除してください
- ユーザー情報は`users.json`ファイルに保存されるため、ファイルのバックアップを定期的に行ってください
- メディアファイルは適切なディレクトリ構造で管理してください

## ライセンス

このプロジェクトはMITライセンスの下で公開されています。詳細は [LICENSE](LICENSE) ファイルを参照してください。

## 開発者向け
設計は<a href="./doc/arch/index.md">doc/arc</a>へ．  
命名規則などのコーディング規約は<a href="./doc/code.md">コーディング規約</a>へ．