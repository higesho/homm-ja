@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
if not exist ..\build mkdir ..\build
rc /nologo /fo ..\build\h3jp.res h3jp.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS /Fo..\build\ main.c font.c text.c testio.c fakedd.c subs.c ..\build\h3jp.res /Fe..\build\wsock32.dll /link /MAP:..\build\h3jp.map /DEF:h3jp.def kernel32.lib user32.lib gdi32.lib advapi32.lib || exit /b 1
