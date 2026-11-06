# Launch debug standalone, click through pages, overwrite docs/*.png.
# Needs a desktop. Tweak $chromeH if the page chevron misses (caption + mute banner).
$ErrorActionPreference = 'Stop'

$chromeH = 70
# Editor layout at uiScale 1: pageBar after title hit (8+150), reduced(4,4), right chevron last 20px.
$nextX = 8 + 150 + 4 + 100 - 10
$nextY = 8 + 4 + 16
$aboutX = 40
$aboutY = 20

Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(int f, int x, int y, int d, int e);
  [StructLayout(LayoutKind.Sequential)]
  struct RECT { public int L, T, R, B; }
  public static bool WindowRect(IntPtr h, out int l, out int t, out int r, out int b) {
    RECT rc; bool ok = GetWindowRect(h, out rc);
    l = rc.L; t = rc.T; r = rc.R; b = rc.B; return ok;
  }
}
"@
[Win]::SetProcessDPIAware() | Out-Null

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe  = Join-Path $root 'Builds\VisualStudio2022\x64\Debug\Standalone Plugin\5HKR.exe'
$out  = Join-Path $root 'docs'
if (-not (Test-Path $exe)) { throw "no debug standalone: $exe — run just build-debug" }

Get-Process -Name 5HKR -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300
$proc = Start-Process $exe -PassThru
$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 50; $i++) {
    Start-Sleep -Milliseconds 200
    $p = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
    if ($p -and $p.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $p.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { throw '5HKR window did not appear' }

function Get-WinRect {
    $l = 0; $t = 0; $r = 0; $b = 0
    [void][Win]::WindowRect($hwnd, [ref]$l, [ref]$t, [ref]$r, [ref]$b)
    return @{ L = $l; T = $t; R = $r; B = $b }
}

function Click-At([int]$x, [int]$y) {
    $r = Get-WinRect
    [void][Win]::ShowWindow($hwnd, 9)
    [void][Win]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 150
    [void][Win]::SetCursorPos($r.L + $x, $r.T + $y)
    [Win]::mouse_event(0x0002, 0, 0, 0, 0)
    [Win]::mouse_event(0x0004, 0, 0, 0, 0)
    Start-Sleep -Milliseconds 250
}

function Save-Shot([string]$name) {
    $r = Get-WinRect
    $w = $r.R - $r.L
    $h = $r.B - $r.T
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path $out "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
}

Start-Sleep -Seconds 1
$pages = @('main', 'mixer', 'effects', 'eq', 'sidechain')
for ($i = 0; $i -lt $pages.Count; $i++) {
    Save-Shot $pages[$i]
    if ($i -lt $pages.Count - 1) { Click-At $nextX ($chromeH + $nextY) }
}
Click-At $aboutX ($chromeH + $aboutY)
Save-Shot 'about'

Stop-Process -Id $proc.Id -Force
Write-Host "wrote $($pages.Count + 1) pngs in $out"
