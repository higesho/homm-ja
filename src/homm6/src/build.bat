@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
if not exist ..\build mkdir ..\build
rc /nologo /fo ..\build\h6jp.res h6jp.rc || exit /b 1
cl /nologo /O2 /MT /W3 /utf-8 /LD /GS- /Fo..\build\ proxy.c h6jp.c ..\build\h6jp.res /Fe..\build\msimg32.dll /link /DEF:msimg32.def kernel32.lib || exit /b 1
