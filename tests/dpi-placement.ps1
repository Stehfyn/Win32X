<#
  dpi-placement.ps1 -- regression test for the ShowWindowEx startup-placement hardening.

  WHAT IT GUARDS
  ShowWindowEx pins the launch measurement/placement to DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 via
  SetThreadDpiAwarenessContextEx so the startup window is sized to three-quarters of the launch monitor's
  *physical* work area regardless of the process's declared DPI awareness. It also exercises the full
  delay-load path at runtime (user32/ole32/advapi32/shell32 bind on first call via our __delayLoadHelper2).

  HOW
  Embeds three different DPI manifests (unaware / system / PerMonitorV2) into copies of WinMainEx.exe with
  mt.exe, launches each, and asserts: (1) the process shows a window (delay-load + thunks work),
  (2) the window's awareness matches the manifest (the variants are genuinely different), and
  (3) window size == 3/4 of the launch monitor's physical work area (placement correct in every mode).

  NOTE ON SCOPE
  The 3/4 *size* ratio is scale-invariant, so this is a regression guard, not an isolation of the pin's
  unique benefit. The pin's distinct value is *positional* -- a physical STARTUPINFO dwX/dwY resolving to
  the correct monitor on a mixed-DPI multi-monitor desktop. That requires a STARTF_USEPOSITION launcher and
  a second monitor; see Test-Position (skipped unless $env:WMX_SECOND_MONITOR is set).

  USAGE:  powershell -NoProfile -ExecutionPolicy Bypass -File tests\dpi-placement.ps1
  EXIT:   0 = all pass, 1 = any failure.
#>

$ErrorActionPreference = 'Stop'
$root   = Split-Path -Parent $PSScriptRoot
$exe    = Join-Path $root 'WinMainEx\x64\Debug\WinMainEx.exe'
$clsid  = '{E5F1A9C2-8B7D-4E3F-A15C-9D2E7B6F4A83}'
$work   = Join-Path $env:TEMP ("wmx_dpitest_" + [System.IO.Path]::GetRandomFileName())
$fail   = 0

function Find-Tool($glob) { (Get-ChildItem $glob -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1).FullName }

# --- build if needed -------------------------------------------------------
if (-not (Test-Path $exe)) {
  $msb = Find-Tool "C:\Program Files\Microsoft Visual Studio\*\*\MSBuild\Current\Bin\MSBuild.exe"
  if (-not $msb) { Write-Error "MSBuild not found and $exe missing"; exit 2 }
  & $msb (Join-Path $root 'WinMainEx\WinMainEx.vcxproj') /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo /v:quiet | Out-Null
}
$mt = Find-Tool "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\mt.exe"
if (-not $mt) { Write-Error "mt.exe not found"; exit 2 }

# --- P/Invoke reader (PMv2, so it reads physical pixels) -------------------
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class WmxProbe {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int l,t,r,b; }
  [StructLayout(LayoutKind.Sequential)] public struct MI { public int cb; public RECT rc; public RECT work; public int flags; }
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr c);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr MonitorFromWindow(IntPtr h, int f);
  [DllImport("user32.dll", EntryPoint="GetMonitorInfoW")] public static extern bool GetMonitorInfo(IntPtr h, ref MI mi);
  [DllImport("user32.dll")] public static extern IntPtr GetWindowDpiAwarenessContext(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetAwarenessFromDpiAwarenessContext(IntPtr c);
  [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
  [DllImport("user32.dll")] public static extern IntPtr MonitorFromPoint(POINT pt, int f);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x,y; }
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] public struct SI {
    public int cb; public IntPtr r1, desktop, title;
    public int dwX,dwY,dwXSize,dwYSize,dwXCount,dwYCount,dwFill,dwFlags;
    public short wShow, cbR2; public IntPtr lpR2, hIn, hOut, hErr; }
  [StructLayout(LayoutKind.Sequential)] public struct PI { public IntPtr hp, ht; public int pid, tid; }
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern bool CreateProcessW(
    string app, IntPtr cmd, IntPtr pa, IntPtr ta, bool inh, int flags, IntPtr env, string dir, ref SI si, out PI pi);
}
"@
[void][WmxProbe]::SetProcessDpiAwarenessContext([IntPtr](-4))   # PER_MONITOR_AWARE_V2

$manifests = @{
  unaware = '<dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">false</dpiAware>'
  system  = '<dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true</dpiAware>'
  pmv2    = '<dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>'
}
$expectAwareness = @{ unaware = 0; system = 1; pmv2 = 2 }   # 0=UNAWARE 1=SYSTEM 2=PER_MONITOR

New-Item -ItemType Directory -Force $work | Out-Null
try {
  foreach ($name in 'unaware','system','pmv2') {
    $mf   = Join-Path $work "$name.manifest"
    $copy = Join-Path $work "WinMainEx_$name.exe"
    $xml  = '<?xml version="1.0" encoding="utf-8"?><assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"><application xmlns="urn:schemas-microsoft-com:asm.v3"><windowsSettings>' + $manifests[$name] + '</windowsSettings></application></assembly>'
    Set-Content -Path $mf -Value $xml -Encoding UTF8
    Copy-Item $exe $copy -Force
    & $mt -nologo -manifest $mf "-outputresource:$copy;#1" | Out-Null

    $proc = Start-Process $copy -PassThru
    try {
      $h = [IntPtr]::Zero
      for ($i = 0; $i -lt 40 -and $h -eq [IntPtr]::Zero; $i++) {
        Start-Sleep -Milliseconds 100
        $p = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
        if ($p) { $h = [IntPtr]$p.MainWindowHandle }
      }
      if ($h -eq [IntPtr]::Zero) { Write-Host "[FAIL] $name : no window (launch/delay-load failed)"; $fail++; continue }

      $r = New-Object WmxProbe+RECT; [void][WmxProbe]::GetWindowRect($h, [ref]$r)
      $mon = [WmxProbe]::MonitorFromWindow($h, 2)
      $mi = New-Object WmxProbe+MI; $mi.cb = [Runtime.InteropServices.Marshal]::SizeOf($mi); [void][WmxProbe]::GetMonitorInfo($mon, [ref]$mi)
      $awEnum = [WmxProbe]::GetAwarenessFromDpiAwarenessContext([WmxProbe]::GetWindowDpiAwarenessContext($h))
      $ww = $r.r - $r.l; $wh = $r.b - $r.t
      $aWid = $mi.work.r - $mi.work.l; $aHgt = $mi.work.b - $mi.work.t
      $rw = $ww / $aWid; $rh = $wh / $aHgt
      $tol = 0.01

      $okAware = ($awEnum -eq $expectAwareness[$name])
      $okSize  = ([math]::Abs($rw - 0.75) -le $tol) -and ([math]::Abs($rh - 0.75) -le $tol)
      if ($okAware -and $okSize) {
        Write-Host ("[PASS] {0,-8}: awareness={1} window={2}x{3} of physWork={4}x{5} (ratio {6:N3}/{7:N3})" -f $name,$awEnum,$ww,$wh,$aWid,$aHgt,$rw,$rh)
      } else {
        if (-not $okAware) { Write-Host ("[FAIL] {0,-8}: awareness {1}, expected {2}" -f $name,$awEnum,$expectAwareness[$name]) }
        if (-not $okSize)  { Write-Host ("[FAIL] {0,-8}: size ratio {1:N3}/{2:N3}, expected 0.750/0.750" -f $name,$rw,$rh) }
        $fail++
      }
    }
    finally {
      Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
      # WinMainEx self-registers the exefile DelegateExecute handler on direct launch; undo it.
      & reg.exe delete "HKCU\Software\Classes\exefile" /f *> $null
      & reg.exe delete "HKCU\Software\Classes\CLSID\$clsid" /f *> $null
    }
  }
}
finally {
  Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
}

if ($fail -eq 0) { Write-Host "`nALL PASS"; exit 0 } else { Write-Host "`n$fail FAILED"; exit 1 }
