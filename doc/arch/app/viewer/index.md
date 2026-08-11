# viewer API・ファイル形式

## 基本単位

viewerは次の順でEntryを構成する。

```text
Collection -> Work -> MediaSet -> Member
```

画像、動画、音声、テキスト、ドキュメントはすべてMemberとして扱う。画像専用のAPIや、音声の`main`・`variant`・`bonus`等のroleは存在しない。

子ディレクトリだけを持つディレクトリは、子がWorkならCollectionとして扱う。直接メディアだけを持つ葉ディレクトリはWorkとし、直接メディアと子ディレクトリが同居するディレクトリは、メディアが付随したCollectionとして扱う。直接メディアを持たず、子が動画葉または音声葉のMediaSetだけで構成される場合は、それらをMediaSetとしてまとめるWorkとする。MediaSet葉に加えて別のディレクトリがある場合もCollectionとして扱い、直下のMediaSet葉と子ディレクトリをそれぞれの子Entryとして表示する。Collectionに付随する直接メディアは、各ファイルを`attached_media: true`の閲覧用WorkとしてCollectionの子にする。この閲覧用Workはディレクトリ分類上のWorkではなく、検索・random・メディアページング・タグ編集の対象にしない。各ディレクトリの分類はそのディレクトリ自身の直下メディアと子構造で決定し、子ディレクトリを親Collectionへ移動する昇格は行わない。画像作品の親Collectionなど、構造上WorkとCollectionの境界が曖昧な場合は`.viewer.json`の`"kind": "work"`または`"kind": "collection"`で明示する。`kind: work`は葉MediaSetだけで構成された境界の曖昧さを解決するために使えるが、直接メディアと子ディレクトリの同居や、孫以下のディレクトリを含む構造をWorkへ変更する指定ではない。これはMediaSetの意味を推測するためのrole指定ではない。

直接ファイルが複数のメディア種別にまたがり、子ディレクトリを持たない場合はmixed Workとする。この場合、直接ファイルごとに独立したMediaSetとMemberを作る。直接ファイルが複数種別にまたがる、または単一種別でも子ディレクトリを持つ場合はCollectionとし、付随する直接ファイルはCollectionの子に閲覧用Workとして表示するが、検索・random・メディアページングの対象にしない。通常の単一種別の葉ディレクトリと動画・音声葉だけで構成されたWorkは、従来どおりWork単位で扱う。

テキストの数字名や桁数、画像・動画の特定ファイル名には意味を持たせない。ZIP内部は起動時のGraphState構築単位にせず、ZIPファイル一つを画像Member一つとして扱う。画像ディレクトリを開いた時だけ、ZIP内の画像を一時的な表示Memberとして名前順に展開する。

## IDと重複

全EntryのIDは同じ64-bit論理ID空間にある。ポインタ値、inode、deviceはIDに使わない。`.viewer.json`にIDがある場合はそれを使い、IDがない旧形式は移行期間中だけパス由来の暫定IDで読む。API上のIDは64-bit値の精度を失わないよう、常に10進文字列で送受信する。

各階層をルビ付きUTF-8名のsort key、自然数順、元のパスの順に並べてDFSする。初めて発見した同一IDを正規Entryとし、後から発見したものは正規Entryへのhidden aliasとして扱う。

- 後発Entryの内容差分は調べない。
- 後発Entryを最新版として採用しない。
- 正規Entryが一時的に読めなくても後発Entryへfallbackしない。
- 正規Entryが次回reloadで消えた場合だけ、次の走査で先に発見されたEntryを正規Entryへ昇格する。
- 通常一覧、検索、randomにはaliasを含めない。
- 管理者または許可された診断要求の`include_hidden=true`ではaliasを`hidden_aliases`として返す。
- alias経由のタグ更新は正規Entryの`.viewer.json`だけを更新する。

symlinkは、リンク元と解決先の両方がroot内にある場合だけ扱う。hardlinkは警告して無視する。root外へ解決されるsymlink、解決不能なsymlink、形式違反は警告ログへ出力する。

## メディアの規則

- 画像: 通常画像は名前順のMemberとして扱う。ZIPは構築時に内部を走査せず、ZIPファイル一つを画像Member一つとして扱う。画像Setを開いた時だけZIP内の画像一覧を取得し、表示Memberとして名前順に展開する。ディレクトリのプレビューなど単一content要求ではZIP内の名前順で先頭画像を返す。
- 動画: 子を持たないディレクトリに1本以上の動画があり、直接ファイルとして動画以外のメディアが任意名の画像1枚以下だけである場合、動画Setとして扱う。子ディレクトリがある場合はCollectionとし、直接動画は付随データとして扱う。メタデータやプレビュー画像は必須ではなく、画像があれば任意名のプレビューMemberとして動画と同じSetに属する。音声、テキスト、ドキュメント、未対応ファイルが同じ葉ディレクトリの直接ファイルとして混在する場合は動画専用形式とはみなさず、認識できる直接ファイルをmixed Workの独立Memberとして扱う。旧形式の単体`.mp4`を救済しない。
- 音声: 音声ファイルだけを持つ葉ディレクトリをMediaSetとし、wav、mp3、加工版、特典などの音声葉だけで構成されたディレクトリを同一Work内の別MediaSetとして扱う。直接音声ファイルと子ディレクトリが同居する場合はCollectionとし、直接音声は付随データとして扱う。ディレクトリ名から役割を推測しない。
- テキスト: 数字名を含め、通常のMemberとして共通ソートする。追加ファイルは次回reloadで同じWorkへ反映する。
- ドキュメント: 追加可能なMember列として扱う。

## API

JSON APIは`api_version`、任意の`request_id`、`data`、`diagnostics`を共通エンベロープにする。旧`/req/img`と`/req/media`のwire互換は提供しない。

```text
GET   /req/viewer
GET   /req/viewer/entries/{id}
GET   /req/viewer/entries/{id}/children
GET   /req/viewer/entries/{id}/archive
GET   /req/viewer/page
GET   /req/viewer/content/{member_id}[?archive_member=...]
GET   /req/viewer/search
GET   /req/viewer/random
GET   /req/viewer/status
PATCH /req/viewer/entries/{id}/metadata
POST  /req/viewer/reload
```

### 起動時の可用性

HTTPサービスはlistenを開始した後にviewer workerを起動し、workerは初回もfilesystemから通常どおり`ScanSnapshot`と`GraphState`を構築する。初回構築が完了してcurrent GraphStateが公開されるまでは、viewerは部分的なデータを返さずout of serviceとして扱う。構築完了後は`ready`へ遷移し、その後のreloadではcurrent GraphStateを保持したままbufferを構築して交換する。

`current` GraphStateがまだ存在しない場合、またはreload中で新規`GraphReadView`を取得できない場合、viewerのEntry取得・検索・content・metadata APIは次の形式で応答する。

```text
HTTP 503
error.code: RELOAD_IN_PROGRESS
error.retryable: true
error.refresh: root
data: null
```

初回走査に失敗した場合も、正常なGraphStateが公開されるまでこの状態を維持する。フロントエンドはこれを空のrootや有効なEntryとして扱わず、再試行可能なサービス停止として扱う。

`GET /req/viewer/status`はGraphStateを取得せず、`data.state`として`ready`、`reloading`、`unavailable`のいずれかだけを返す。初回構築後は`ready`へ遷移し、reload要求の受付から公開完了までは`reloading`を返す。reload中もcurrent GraphStateが存在する限りEntry APIはそれを読み取れるため、初回走査完了後のreloadでは既存画面を維持できる。reload中のフロントエンドは既存画面を保持し、この状態だけを端部の通知で示す。

Entry詳細は最小構成とし、子Entryは`children`で取得する。ディレクトリ表示は`children?all=true`で現在ディレクトリの到達可能な直下の子を一覧し、`page`とは別の表示モードとして同じ一覧コンテナを排他的に利用する。`attached_media: true`のWorkはCollection直下のメディアを閲覧するための子Entryであり、通常のディレクトリWorkではない。これは`children`では取得できるが、検索・random・`page`・タグ編集の対象外である。`page`は通常のWorkとmixed Work内の独立Memberを対象にした、全メディアまたはメディア種別ごとの更新時刻順キャッシュをページングする。`page`の`filter=all|image|video|audio|text|document`はキャッシュを選び、既定の更新日時降順ではキャッシュ順をそのまま使う。名前順や更新日時昇順を要求した場合だけ、選択された全キャッシュをその順に並べ替えてからページングする。検索は全Workを逐次走査して`page`、`limit`、`sort_key=path|updated_at`、`direction=asc|desc`、`filter`で絞り込む。`random`はページングせず、指定種別のキャッシュから少数の固定個数を無作為に返す。`grouping=mixed`は全種別を一列で比較し、`grouping=media_type`はディレクトリ、画像、動画、音声、テキスト、ドキュメントの固定グループ順で比較する。グループ内だけ方向指定を反映する。WorkとCollectionの`media_types`は要求者から到達可能な子だけを集約した種別要約である。Workを開いたときの初期MediaSetは要求された順序の先頭である。ページング／randomでMemberが返された場合も、`parent_id`からMediaSetとWorkを解決して通常のWork表示を開く。更新時刻も精度を失わない文字列で表現する。`display_name`は有効な末尾ルビ注記を除いた表示本体、`display_name_ruby`はルビ文字列であり、HTMLは返さない。管理者の開発者メニューは`include_hidden=true`を付け、`hidden_aliases`を表示できる。

未知IDやreload中の参照は空JSONにせず、`STALE_REFERENCE`、`RELOAD_IN_PROGRESS`、`SOURCE_UNAVAILABLE`等の機械可読エラーを返す。フロントエンドはエラーの`refresh`に従ってEntryまたはrootを再取得する。

## reloadとタグ

GraphStateは`current`と`buffer`の二つを持ち、reloadごとにbufferを空から構築する。reload開始時は新規GraphReadViewを止め、旧GraphStateを読むサーバー側リクエストの終了を待つ。構築後にGraphStateを交換し、古いarenaを読者がいなくなるまで再利用しない。

タグ操作はGraphUpdaterのキューで直列化し、追加・削除は冪等とする。reloadとタグ操作が同時に到着しても、正規Entry解決後のGraphStateと`.viewer.json`が同じ操作結果へ収束する。

reloadは管理者だけが実行でき、最低待機時間未満の要求は`RELOAD_COOLDOWN`、実行中の重複要求は`RELOAD_ALREADY_PENDING`として返す。dirty flagと定期走査による自動reloadも同じ経路を使う。reload開始時点までに発生したdirtyはその走査へ含まれるため消費し、走査開始後から公開完了までに発生したdirtyはフラグを保持して、最低待機時間を満たした後に次のreloadを一度だけ実行する。複数回のdirtyは一つに畳み込む。reload失敗時もdirtyを保持して再試行する。定期スキャン間隔は`VIEWER_SCAN_INTERVAL_SECONDS`で秒単位に指定し、未指定時の既定値は3時間（10800秒）とする。0以下は設定エラーとする。

非管理者の公開規則は`VIEWER_PUB_LIST`の順序付き相対パス規則で、既定deny、後勝ち、先頭`!`がdenyである。子を許可する規則は到達に必要な祖先のトラバーサルだけを暗黙に許可し、symlinkはリンク元と解決先の双方を評価する。

## フロントエンド構成

フロントエンドはviewer専用の状態・API・描画を分離する。

- `api/client.js`: `/req/viewer`契約、認証、共通エラー、content取得。
- `state/store.js`: ready/loading/unavailable/error、検索、選択中Member、操作キャンセルを管理する。
- `view/ordering.js`: ルビ・自然数・メディアグループ順の表示比較を担当する。
- `view/media.js`: Member種別ごとの表示要素を生成する。
- `view/render.js`: DOM描画だけを担当し、ユーザー入力をHTMLとして解釈しない。
- `main.js`: イベント、ナビゲーション、検索、タグ更新、reloadを接続し、古い操作の結果を世代トークンとAbortControllerで破棄する。
- `style.css`: 外部フォントやアイコンに依存しない、viewer専用の最小スタイル。

初回GraphStateがない起動時だけ画面全体をサービス停止表示にし、reload中は古いEntry、CollectionContainer、MediaSetsContainer、メインメディアを保持する。フロントエンドは一覧ごとに独立したsort、direction、grouping、filter、page状態を持ち、自動移動、メディア終了時の再生モード（ループ・自動遷移・何もしない）、音量、MediaMembersの開閉、現在のEntry IDも再読み込み後に復元する。MediaSetsとMediaMembersはpage状態を持たず、全件取得して各コンテナ内をスクロールする。横長viewportではメイン表示とMediaMembersを横並びにし、MediaSetsをその下、Collectionの上に置く。縦長viewportでもMediaSetsはメイン表示の下に置き、MediaMembersだけを左右ドロワーにしてメイン表示が全幅を使う。Entry IDはDOMやURLで文字列として扱い、64bit数値へ変換しない。作品選択はメインコンテンツの中央へ、Collection選択は`#THUM`へ移動する。ディレクトリ表示とページングは同じ内容コンテナを共有するが、同時には表示しない。ページングと検索の取得数は固定値にせず、取得直前に画面の幅とグリッド列数から計算し、横長viewportでは2行、縦長viewportでは4行を表示する。randomはページングではないため、この行数規則を適用しない。ページングが表示中は左右矢印キーでも前後ページへ移動できるが、入力欄・音声・動画操作中のキー入力は奪わない。
