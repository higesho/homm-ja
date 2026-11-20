#!/bin/sh
# build/wsock32.dll from src/ (MSVC x86)
cd "$(dirname "$0")/.."
powershell -NoProfile -Command "cmd /c '\"$(cygpath -w "$PWD")\src\build.bat\"'" 2>&1 | grep -E -i "error|warning C|LNK" ; ls -la build/wsock32.dll
