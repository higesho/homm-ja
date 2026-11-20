# Heroes of Might and Magic 日本語化パッチ（プログラム）

DOSBox 用の winmm.dll です。起動時に HEROES.EXE と地図・AGG を日本語版に差し替え、文字の描画を日本語に対応させます。

## 組み立て方
1. `python tools/make_patch.py` で build/patch.bin（字形・訳・差分）を作る
2. `src\build.bat` で build/winmm.dll を作る
3. winmm.dll をゲームの DOSBOX フォルダに置く

## 用意するもの
- Windows、Visual Studio 2022（cl・rc）、Python 3
- 原作のゲーム（GOG 版）。原作のファイルはここには入っていないので、各自用意してください。
- 訳のデータとフォントの元データもここには入っていません。

## ライセンス
MIT（リポジトリ直下の LICENSE）。
