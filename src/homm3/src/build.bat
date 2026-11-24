@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
set O=..\build\release
set OUT=..\build\wsock32.dll
set DEF=
set EXTRA=
if "%1"=="test" (
  set O=..\build\test
  set OUT=..\build\test\wsock32.dll
  set DEF=/DH3JP_TEST
  set EXTRA=testio.c fakedd.c
)
if not exist %O% mkdir %O%
rc /nologo /fo %O%\h3jp.res h3jp.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS %DEF% /Fo%O%\ main.c font.c text.c subs.c %EXTRA% %O%\h3jp.res /Fe%OUT% /link /MAP:%O%\h3jp.map /DEF:h3jp.def kernel32.lib user32.lib gdi32.lib advapi32.lib || exit /b 1
