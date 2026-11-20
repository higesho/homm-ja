# Heroes of Might and Magic III Complete 日本語化パッチ（プログラム）

wsock32.dll の代理 DLL です。文字の描画関数を差し替え、TXT・地図・キャンペーンの文と動画の字幕を日本語にします。

## 組み立て方
1. `python tools/extract.py` で英語の文を取り出す
2. `sh tools/rebuild.sh` で訳をまとめ、字形を作り、build/wsock32.dll を作る
3. wsock32.dll をゲームのフォルダに置く

## 用意するもの
- Windows、Visual Studio 2022（cl・rc）、Python 3
- 原作のゲーム（GOG 版。Horn of the Abyss にも対応）。原作のファイルはここには入っていないので、各自用意してください。
- 訳のデータとフォントの元データもここには入っていません。

## ライセンス
MIT（リポジトリ直下の LICENSE）。
