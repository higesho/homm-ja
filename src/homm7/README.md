# Might & Magic Heroes VII 日本語化パッチ（プログラム）

日本語の言語ファイル（JPN）とフォントのパッケージを作る道具と、日本語を選ばせる dinput8.dll です。

## 組み立て方
1. `python tools/build.py` で言語ファイルを作る
2. `python tools/make_fonts.py all` でフォントのパッケージを作る
3. `sh tools/build_dll.sh` で build/dinput8.dll（配布版）と build/test/dinput8.dll（試験版。/DH7TEST 付きで tools/win.py が使う。配布しない）を作る（64 ビット）

## 用意するもの
- Windows、Visual Studio 2022（cl・rc）、Python 3
- 原作のゲーム（Steam 版）。原作のファイルはここには入っていないので、各自用意してください。
- 訳のデータとフォントの元データもここには入っていません。
- パッケージの LZO 展開の部分（tools/lzo1x.py）は入っていません。upkdec.py がこれを読み込むので、各自用意が必要です。

## ライセンス
MIT（リポジトリ直下の LICENSE）。
