# Contributing to HOME-SERVER

このプロジェクトへの貢献をありがとうございます！このドキュメントは、プロジェクトへの貢献方法を説明します。

## 🚀 貢献の流れ

### 1. Issue の作成

バグ報告や機能要望がある場合は、まず Issue を作成してください：

- **バグ報告**: 再現手順、期待される動作、実際の動作を記載
- **機能要望**: 機能の詳細、使用例、実装案を記載
- **ドキュメント改善**: 改善したい箇所と理由を記載

### 2. 開発環境のセットアップ

```bash
# リポジトリのクローン
git clone https://github.com/clever-elsie/archive.git
cd archive

# 開発用ブランチの作成
git checkout -b feature/your-feature-name

# 開発環境での実行
docker-compose --profile dev up --build
```

### 3. コードの実装

#### コーディング規約

- **C++23標準**: 最新のC++標準に準拠
- **命名規則**: 
  - クラス名: `PascalCase`
  - 関数名: `snake_case`
  - 定数: `UPPER_SNAKE_CASE`
- **コメント**: 複雑なロジックには適切なコメントを追加
- **エラーハンドリング**: 適切な例外処理とエラーメッセージ
- **推奨事項**: メンバ関数には`const`や`noexcept`などの修飾を適切に追加する

#### ファイル構造

```
src/
├── server/           # バックエンド (C++)
│   ├── *.hpp        # ヘッダーファイル
│   └── *.cpp        # 実装ファイル
└── frontend/         # フロントエンド
    ├── viewer/       # メディアビューアー
    └── memo/         # メモ管理
```

### 4. テスト

- **単体テスト**: 新機能には単体テストを追加
- **統合テスト**: API エンドポイントのテスト
- **手動テスト**: ブラウザでの動作確認

### 5. コミット

```bash
# 変更の確認
git status
git diff

# コミット
git add .
git commit -m "feat: add new feature description"

# プッシュ
git push origin feature/your-feature-name
```

#### コミットメッセージの規約

- `feat:` - 新機能
- `fix:` - バグ修正
- `docs:` - ドキュメント更新
- `style:` - コードスタイルの修正
- `refactor:` - リファクタリング
- `test:` - テストの追加・修正
- `chore:` - その他の変更

### 6. Pull Request

1. GitHub で Pull Request を作成
2. タイトルは簡潔で分かりやすく
3. 説明には変更内容と理由を記載
4. 関連する Issue にリンク

## 🔧 開発環境
Ubuntu24.04での動作を想定していますが，他のLinuxで動かない訳ではないと思います．

### 必要なツール

- **C++23対応コンパイラ**: GCC 13+ または Clang 16+ (dockerではGCCを利用しますがGCC拡張構文は禁止)
- **CMake**: 3.25+
- **Crow** webフレームワーク
- **OpenSSL**: 開発ライブラリ
- **Docker**: コンテナ化環境
- **Git**: バージョン管理

### ローカル開発

```bash
# 依存関係のインストール
sudo apt install build-essential cmake libssl-dev pkg-config libasio-dev

# ビルド
mkdir build && cd build
cmake ..
make

# 実行
./server_systemd.out
```
systemdに登録して使うことを想定しています．
`.service`の書き方は`scripts/home-server.service.in`を参考にしてください．

### Docker 開発環境

```bash
# 開発環境の起動
docker-compose --profile dev up --build

# ログの確認
docker-compose logs -f home-server-dev

# コンテナ内での実行
docker-compose --profile dev exec home-server-dev bash
```

## 📋 チェックリスト

Pull Request を送信する前に、以下を確認してください：

### コード品質
- [ ] コーディング規約に準拠
- [ ] 適切なコメントとドキュメント
- [ ] エラーハンドリングの実装
- [ ] セキュリティを考慮した実装

### テスト
- [ ] 単体テストの追加・更新
- [ ] 統合テストの実行
- [ ] 手動テストの実施
- [ ] 既存機能への影響確認

### ドキュメント
- [ ] README.md の更新（必要に応じて）
- [ ] API ドキュメントの更新
- [ ] コメントの追加・更新

### セキュリティ
- [ ] 入力値の検証
- [ ] SQL インジェクション対策
- [ ] XSS 対策
- [ ] CSRF 対策

## 🐛 バグ報告

バグを発見した場合は、以下の情報を含めて Issue を作成してください：

### 必須情報
- **バグの概要**: 簡潔な説明
- **再現手順**: 詳細な手順
- **期待される動作**: 正常な動作
- **実際の動作**: 現在の動作
- **環境情報**: OS、ブラウザ、バージョン

### オプション情報
- **スクリーンショット**: 視覚的な証拠
- **ログファイル**: エラーログ
- **関連する Issue**: 既存の類似 Issue

## 💡 機能要望

新機能の提案がある場合は、以下の情報を含めてください：

### 必須情報
- **機能の概要**: 何を実現したいか
- **使用例**: 具体的な使用場面
- **実装案**: 技術的な実装方法
- **優先度**: 高・中・低

### オプション情報
- **UI/UX の提案**: デザイン案
- **API 設計**: エンドポイントの設計
- **データベース設計**: スキーマの変更

## 📞 サポート

質問や相談がある場合は、以下でお気軽にお声かけください：

- **GitHub Issues**: 技術的な質問
- **GitHub Discussions**: 一般的な議論
- **Email**: 直接連絡が必要な場合

## 📄 ライセンス

このプロジェクトは MIT ライセンスの下で公開されています。貢献するコードも同じライセンスの下で公開されることにご同意ください。

---

ご協力ありがとうございます！🎉 