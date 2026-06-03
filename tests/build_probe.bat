@echo off
call "C:\Program Files\Microsoft Visual Studio‚2\Community\VC\Auxiliary\Buildcvars64.bat" >nul
cl /nologo /W3 /O2 dxgi_probe.c /Fe:dxgi_probe.exe
