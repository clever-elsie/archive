# コーディング規約
## 命名規則
### C++
- 名前空間 : `UPPERCASE` の単語．  
  複数語になる場合は`_`で区切る．  
  `namespace archive`は特別に小文字とする．
- クラス・関数・変数名 : `snake_case`．  

### HTML/CSS
### JavaScript

## 設計
### C++
ヘッダファイルを`includes`に，ソースファイルを`src/server`に配置する．
+ `manager` : 通信，認証など，アプリケーションを利用する前提となる処理を行う．
+ `app` : サーバーで処理を行うアプリケーションを配置する．
+ `lib` : `manager`,`app`を問わず利用できる抽象的なプログラム

### HTML/CSS
### JavaScript