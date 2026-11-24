# Heroes of Might and Magic III Complete 日本語化パッチ（プログラム）

wsock32.dll の代理 DLL です。文字の描画関数を差し替え、TXT・地図・キャンペーンの文と動画の字幕を日本語にします。

## 組み立て方
1. `python tools/extract.py` で英語の文を取り出す
2. `sh tools/rebuild.sh` で訳をまとめ、字形を作り、build/wsock32.dll（配布版）と build/test/wsock32.dll（試験版。tools/win.py が使う。配布しない）を作る。DLL だけを作り直すときは `src\build.bat`（配布版）、`src\build.bat test`（試験版）
3. wsock32.dll をゲームのフォルダに置く

## 用意するもの
- Windows、Visual Studio 2022（cl・rc）、Python 3
- 原作のゲーム（GOG 版。Horn of the Abyss にも対応）。原作のファイルはここには入っていないので、各自用意してください。
- 訳のデータとフォントの元データもここには入っていません。

## ライセンス
MIT（リポジトリ直下の LICENSE）。
