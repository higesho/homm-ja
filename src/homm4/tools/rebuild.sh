#!/bin/sh
# build/wsock32.dll（配布版）と build/test/wsock32.dll（試験版。tools/win.py が使う。配布しない）from src/ (MSVC x86)
cd "$(dirname "$0")/.."
powershell -NoProfile -Command "cmd /c '\"$(cygpath -w "$PWD")\src\build.bat\"'" 2>&1 | grep -E -i "error|warning C|LNK"
powershell -NoProfile -Command "cmd /c '\"$(cygpath -w "$PWD")\src\build.bat\" test'" 2>&1 | grep -E -i "error|warning C|LNK"
ls -la build/wsock32.dll build/test/wsock32.dll
