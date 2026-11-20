@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
if not exist ..\build mkdir ..\build
cl /nologo /utf-8 /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS /Fo..\build\ proxy.c h5t.c testio.c /Fe..\build\dinput8.dll /link /DEF:dinput8.def kernel32.lib user32.lib gdi32.lib shell32.lib dxguid.lib || exit /b 1
