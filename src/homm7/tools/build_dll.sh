#!/bin/sh
# Build build/dinput8.dll (release) and build/test/dinput8.dll (/DH7TEST: input injection for tools/win.py).
# The game path contains '&', which cmd.exe cannot handle; build in a temp copy.
J="$(cd "$(dirname "$0")/.." && pwd)"
T="${TEMP:-/tmp}/h7jp_build"
rm -rf "$T"; mkdir -p "$T"
cp "$J"/src/*.c "$J"/src/*.inc "$J"/src/*.def "$T"/
cat > "$T/b.bat" <<'B'
@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
mkdir test 2>nul
cl /nologo /O2 /MT /W3 /utf-8 /LD /GS- h7jp.c /Fedinput8.dll /link /DEF:dinput8.def kernel32.lib user32.lib || exit /b 1
cl /nologo /O2 /MT /W3 /utf-8 /LD /GS- /DH7TEST /D_CRT_SECURE_NO_WARNINGS h7jp.c /Fotest\ /Fetest\dinput8.dll /link /DEF:dinput8.def kernel32.lib user32.lib dxguid.lib || exit /b 1
B
cmd //c "$(cygpath -w "$T")\b.bat" || exit 1
mkdir -p "$J/build/test"; cp "$T/dinput8.dll" "$J/build/"; cp "$T/test/dinput8.dll" "$J/build/test/"
ls -la "$J/build/dinput8.dll" "$J/build/test/dinput8.dll"
