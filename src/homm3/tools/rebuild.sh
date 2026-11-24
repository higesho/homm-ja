#!/bin/sh
# merge translations and rebuild build/wsock32.dll（配布版）と build/test/wsock32.dll（試験版）
set -e
cd "$(dirname "$0")/.."
export PYTHONIOENCODING=utf-8
python tools/glossary.py > /dev/null
python tools/merge.py
python tools/make_data.py
python tools/make_subs.py > /dev/null
[ build/glyphs.bin -nt tools/make_font.py ] || python tools/make_font.py
cmd //c "src\build.bat" 2>&1 | grep -iE "error|warning" || true
# 試験版（tools/win.py が使う。配布しない）
cmd //c "src\build.bat test" 2>&1 | grep -iE "error|warning" || true
ls -la build/wsock32.dll build/test/wsock32.dll
