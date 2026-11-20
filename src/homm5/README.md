# Heroes of Might and Magic V 日本語化パッチ（プログラム）

日本語の文とフォントを入れた .pak を作る道具です。src/ の dinput8.dll は確認用です。

## 組み立て方
1. `python tools/extract.py` で英語の文を取り出す
2. `python tools/build.py tote h5 hof` で .pak を作る（フォントは tools/make_fonts.py）
3. `python tools/make_zip.py` で配布用にまとめる

## 用意するもの
- Windows、Visual Studio 2022（cl・rc）、Python 3
- 原作のゲーム（GOG 版。Tribes of the East・Hammers of Fate・無印）。原作のファイルはここには入っていないので、各自用意してください。
- 訳のデータとフォントの元データもここには入っていません。

## ライセンス
MIT（リポジトリ直下の LICENSE）。
