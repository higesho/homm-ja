# Might & Magic Heroes VI 日本語化パッチ（プログラム）

日本語の文とフォントを入れた Data23.orc を作る道具と、英語のままでも訳を読み込ませる msimg32.dll です。

## 組み立て方
1. `python tools/extract.py` で英語の文を取り出す
2. `python tools/build.py` で Data23.orc を作る
3. `src\build.bat` で build/msimg32.dll を作る（代理の部分は tools/gen_proxy.py）
4. 2 つをゲームのフォルダに置く

## 用意するもの
- Windows、Visual Studio 2022（cl・rc）、Python 3
- 原作のゲーム（Steam 版 2.1.1）。原作のファイルはここには入っていないので、各自用意してください。
- 訳のデータとフォントの元データもここには入っていません。
- .orc の暗号を解く部分（tools/orc.py）は入っていません。extract.py・build.py などはこれを読み込むので、各自用意が必要です。

## ライセンス
MIT（リポジトリ直下の LICENSE）。
