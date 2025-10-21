# shared_mutex
`shared_mutex`はReader/Writer処理を実現するためのクラス．  
処理方式にはRead/Write/Interruptible Write(IW)の3種類がある．  
Readは同時に複数読み込みでき，Writeは唯一の排他処理を行う．  
IWはReadよりも優先度が低いため割込み可能なWrite処理である．  
タスクはFIFOで処理されるが，IWがある時はREADは常に優先される．  
IWはあくまでもWriteの特殊なケースなので，IW実行中はWriteはブロックされる．

```C++
namespace archive{

class shared_mutex;

}
```
## 目次
|名前|説明|
|-|-|
|<a href='#constructor'>(constructor)</a>|コンストラクタ|
|<a href='#destructor'>(destructor)</a>|デストラクタ|
|(operator=)|`=delete`|
|<a href='#weak_lock'>`weak`系</a>|共有割込み可能排他ロック|
|<a href='#lock_shared'>`lock_shared`系</a>|共有ロック|
## constructor
```C++
shared_mutex();
```
デフォルト構築のみ許す．  
コピーやmoveはできない．
## destructor
```C++
~shared_mutex();
```
ロック中の破棄は未定義動作．
## lock
```C++
```
## lock_shared
```C++
```