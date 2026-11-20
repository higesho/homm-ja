@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
if not exist ..\build mkdir ..\build
cl /nologo /O2 /MT /W3 /utf-8 /LD /GS- /Fo..\build\ proxy.c h6jp.c /Fe..\build\msimg32.dll /link /DEF:msimg32.def kernel32.lib || exit /b 1
