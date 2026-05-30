# prebuild.ps1 — dev pre-build step for WinMainEx.
#
# WinMainEx registers itself as the exefile "open" DelegateExecute COM handler, so
# rpcss keeps a resident -Embedding server alive that holds WinMainEx.exe open. That
# makes the linker fail with LNK1104 ("cannot open file ... WinMainEx.exe") on every
# rebuild. This script clears that state before each build:
#   1. kill any running WinMainEx instance (frees the exe for the linker)
#   2. unregister the handler (so rpcss doesn't respawn -Embedding and relock it)
#
# The app self-registers again on the next direct launch (run_direct -> do_register),
# so this only affects the build, not testing.

$ErrorActionPreference = 'SilentlyContinue'
$clsid = '{E5F1A9C2-8B7D-4E3F-A15C-9D2E7B6F4A83}'

# 1. kill running instances
Get-Process WinMainEx -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

# 2. unregister (HKCU): exefile override + the COM class
Remove-Item -Path 'HKCU:\Software\Classes\exefile'        -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path "HKCU:\Software\Classes\CLSID\$clsid"   -Recurse -Force -ErrorAction SilentlyContinue

# never fail the build
exit 0
