# Heroes of Might and Magic IV Complete 日本語化パッチ（プログラム）

wsock32.dll の代理 DLL です。フォントの関数を差し替え、表・文字列ファイル・地図の文を日本語にします。

## 組み立て方
1. `python tools/extract.py` で英語の文を取り出す
2. `python tools/make_tr.py` で訳の表、`python tools/make_font.py` でフォントを作る
3. `sh tools/rebuild.sh` で build/wsock32.dll（配布版）を作り、ゲームのフォルダに置く。同時に試験版の build/test/wsock32.dll（`src\build.bat test` で作るもの。tools/win.py が使う。配布しない）もできる

## 用意するもの
- Windows、Visual Studio 2022（cl・rc）、Python 3
- 原作のゲーム（GOG 版）。原作のファイルはここには入っていないので、各自用意してください。
- 訳のデータとフォントの元データもここには入っていません。

## ライセンス
MIT（リポジトリ直下の LICENSE）。
