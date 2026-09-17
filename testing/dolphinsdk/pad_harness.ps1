# tev-one-op test harness (pureikyubu SDL2 build).
#
# Runs a DolphinSDK demo, drives the emulated GameCube pad through the emulator's own SDL
# keyboard bindings (synthetic keys are injected with SendInput, which reaches the focused
# window; PostMessage is ignored by SDL2), captures the "Video Output" window with
# PrintWindow(PW_RENDERFULLCONTENT) and saves the EMU_LOG.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File harness.ps1 -Demo <elf> -OutDir <dir> [-Steps <json>]
#
# The steps file is a JSON array: { "name": "...", "keys": ["X","RIGHT",...], "hold": ms, "wait": ms }

param(
    [Parameter(Mandatory=$true)][string]$Demo,
    [Parameter(Mandatory=$true)][string]$OutDir,
    [string]$WorkDir = "C:\Work\pureikyubu\build",
    [string]$Exe = "C:\Work\pureikyubu\build\pureikyubu_sdl.exe",
    [int]$WindowTimeoutMs = 25000,
    [int]$SettleMs = 6000,
    [string]$Steps = "",
    [switch]$NoKill
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Win {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern uint SendInput(uint n, INPUT[] inp, int size);
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    public struct POINT { public int X; public int Y; }
    [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk; public ushort wScan; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx; public int dy; public uint mouseData; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)] public struct HARDWAREINPUT { public uint uMsg; public ushort wParamL; public ushort wParamH; }
    [StructLayout(LayoutKind.Explicit)] public struct INPUTUNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; [FieldOffset(0)] public HARDWAREINPUT hi; }
    [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public INPUTUNION u; }
    public static void Key(ushort scan, bool extended, bool up) {
        INPUT[] a = new INPUT[1];
        a[0].type = 1;
        a[0].u.ki.wVk = 0;
        a[0].u.ki.wScan = scan;
        a[0].u.ki.dwFlags = (uint)(0x0008 | (extended ? 0x0001 : 0x0000) | (up ? 0x0002 : 0x0000));
        a[0].u.ki.time = 0;
        a[0].u.ki.dwExtraInfo = IntPtr.Zero;
        SendInput(1, a, Marshal.SizeOf(typeof(INPUT)));
    }
}
"@

# The keys of the demo, as the emulator's SDL bindings name them (SettingsSdl.json):
#   pad X -> S, pad Y -> A, pad R -> W, pad A -> X, START -> Enter,
#   stick -> the arrow keys, D-PAD -> Home/End/Delete/PageDown.
$KEYMAP = @{
    "X"          = @(0x1F, $false)
    "Y"          = @(0x1E, $false)
    "R"          = @(0x11, $false)
    "A"          = @(0x2D, $false)
    "START"      = @(0x1C, $false)
    "UP"         = @(0x48, $true)
    "DOWN"       = @(0x50, $true)
    "LEFT"       = @(0x4B, $true)
    "RIGHT"      = @(0x4D, $true)
    "DPAD_UP"    = @(0x47, $true)
    "DPAD_DOWN"  = @(0x4F, $true)
    "DPAD_LEFT"  = @(0x53, $true)
    "DPAD_RIGHT" = @(0x51, $true)
}

function Find-VideoWindow([int]$pid_) {
    $script:found = [IntPtr]::Zero
    $cb = [Win+EnumProc]{
        param($h, $l)
        if (-not [Win]::IsWindowVisible($h)) { return $true }
        $procId = 0
        [void][Win]::GetWindowThreadProcessId($h, [ref]$procId)
        if ($procId -ne $pid_) { return $true }
        $sb = New-Object System.Text.StringBuilder 512
        [void][Win]::GetWindowText($h, $sb, 512)
        if ($sb.ToString() -eq "Video Output") { $script:found = $h; return $false }
        return $true
    }
    [void][Win]::EnumWindows($cb, [IntPtr]::Zero)
    return $script:found
}

function Save-Shot([IntPtr]$hwnd, [string]$out) {
    if ([Win]::IsIconic($hwnd)) { [void][Win]::ShowWindow($hwnd, 9) }
    $wr = New-Object Win+RECT
    [void][Win]::GetWindowRect($hwnd, [ref]$wr)
    $cr = New-Object Win+RECT
    [void][Win]::GetClientRect($hwnd, [ref]$cr)
    $ww = $wr.Right - $wr.Left
    $wh = $wr.Bottom - $wr.Top
    $cw = $cr.Right - $cr.Left
    $ch = $cr.Bottom - $cr.Top
    if ($ww -le 0 -or $wh -le 0 -or $cw -le 0 -or $ch -le 0) { return "badrect" }
    $pt = New-Object Win+POINT
    $pt.X = 0; $pt.Y = 0
    [void][Win]::ClientToScreen($hwnd, [ref]$pt)
    $ox = $pt.X - $wr.Left
    $oy = $pt.Y - $wr.Top
    $full = New-Object System.Drawing.Bitmap $ww, $wh
    $g = [System.Drawing.Graphics]::FromImage($full)
    $hdc = $g.GetHdc()
    $ok = [Win]::PrintWindow($hwnd, $hdc, 2)
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    if (-not $ok) { $full.Dispose(); return "printwindow-failed" }
    $client = New-Object System.Drawing.Bitmap $cw, $ch
    $gc = [System.Drawing.Graphics]::FromImage($client)
    $gc.DrawImage($full, (New-Object System.Drawing.Rectangle 0, 0, $cw, $ch),
                  (New-Object System.Drawing.Rectangle $ox, $oy, $cw, $ch),
                  [System.Drawing.GraphicsUnit]::Pixel)
    $gc.Dispose()
    $full.Dispose()
    $client.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
    $client.Dispose()
    return "ok"
}

function Press-Key([IntPtr]$hwnd, [string]$name, [int]$holdMs) {
    if (-not $KEYMAP.ContainsKey($name)) { Write-Host "  unknown key '$name'"; return $false }
    [void][Win]::SetForegroundWindow($hwnd)
    $scan = $KEYMAP[$name][0]
    $ext = $KEYMAP[$name][1]
    [Win]::Key([uint16]$scan, [bool]$ext, $false)
    Start-Sleep -Milliseconds $holdMs
    [Win]::Key([uint16]$scan, [bool]$ext, $true)
    Start-Sleep -Milliseconds 200
    return $true
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$log = Join-Path $WorkDir "emu_log.txt"
$env:EMU_LOG = $log
if (Test-Path $log) { Remove-Item $log -Force }

$proc = Start-Process -FilePath $Exe -ArgumentList @($Demo) -WorkingDirectory $WorkDir -PassThru
Write-Host "started pid $($proc.Id)"

$hwnd = [IntPtr]::Zero
$waited = 0
while ($waited -lt $WindowTimeoutMs) {
    Start-Sleep -Milliseconds 400
    $waited += 400
    if ($proc.HasExited) { Write-Host "process exited early"; break }
    $hwnd = Find-VideoWindow $proc.Id
    if ($hwnd -ne [IntPtr]::Zero) { break }
}
if ($hwnd -eq [IntPtr]::Zero) { Write-Host "NO WINDOW"; if (-not $proc.HasExited) { $proc.Kill() }; exit 2 }
Write-Host "window $hwnd after ${waited}ms"

[void][Win]::ShowWindow($hwnd, 9)
[void][Win]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds $SettleMs

$r = Save-Shot $hwnd (Join-Path $OutDir "000_baseline.png")
Write-Host "baseline shot: $r"

if ($Steps -ne "") {
    # NB: not $steps - variable names are case-insensitive and [string]$Steps would coerce it.
    $stepList = Get-Content $Steps -Raw | ConvertFrom-Json
    $i = 0
    foreach ($s in $stepList) {
        $i++
        Write-Host ("step {0}: {1}" -f $i, $s.name)
        $hold = 150
        if ($s.hold) { $hold = [int]$s.hold }
        foreach ($k in $s.keys) {
            [void](Press-Key $hwnd $k $hold)
            Start-Sleep -Milliseconds 500
        }
        $wait = 1200
        if ($s.wait) { $wait = [int]$s.wait }
        Start-Sleep -Milliseconds $wait
        $shot = Join-Path $OutDir ("{0:d3}_{1}.png" -f $i, $s.name)
        $r = Save-Shot $hwnd $shot
        Write-Host "  shot: $r -> $shot"
    }
}

if (-not $NoKill) {
    if (-not $proc.HasExited) { $proc.Kill(); $proc.WaitForExit() }
}
$logDst = Join-Path $OutDir "emu_log.txt"
if (Test-Path $log) { Copy-Item $log $logDst -Force }
Write-Host "DONE"
