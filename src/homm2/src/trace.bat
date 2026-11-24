@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
cl /nologo /O2 /MT /LD /DPROXY_TRACE /D_CRT_SECURE_NO_WARNINGS /Fo..\build\ proxy.c h2jp.c ..\build\obj\h2jp.res /Fe..\build\trace.dll /link /DEF:winmm.def kernel32.lib user32.lib
