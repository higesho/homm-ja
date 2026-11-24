@echo off
rem 引数なし: 配布版を ..\build\winmm.dll に作る
rem build.bat test: 試験版（/DH2JP_TEST）を ..\build\test\winmm.dll に作る（配布しない）
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
set O=..\build\obj
set OUT=..\build\winmm.dll
set DEF=
if "%1"=="test" (
  set O=..\build\test\obj
  set OUT=..\build\test\winmm.dll
  set DEF=/DH2JP_TEST
)
if not exist %O% mkdir %O%
rc /nologo /fo %O%\h2jp.res h2jp.rc || exit /b 1
cl /nologo /O2 /MT /W3 /LD /utf-8 /D_CRT_SECURE_NO_WARNINGS %DEF% /Fo%O%\ proxy.c h2jp.c %O%\h2jp.res /Fe%OUT% /link /DEF:winmm.def /IMPLIB:%O%\winmm.lib kernel32.lib user32.lib || exit /b 1
