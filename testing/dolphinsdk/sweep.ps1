# Sweep DolphinSDK demos through the pureikyubu SDL2 port (issue #385).
#
# For every demo: mount the SDK as a virtual DVD (autoexec.cmd), start the demo, wait for the
# "Video Output" window, capture the window's own client area to a PNG, save the EMU_LOG, kill it.
#
# The capture uses PrintWindow(PW_RENDERFULLCONTENT), so the result is the window's own content and
# does not depend on the window being in front or unobscured (CopyFromScreen would grab whatever
# happens to be on top of it). A software-screen fallback is only used when PrintWindow comes back
# blank.
#
# Usage: powershell -ExecutionPolicy Bypass -File piku_sweep2.ps1 -List demos.txt -OutDir C:\Work\sweep
param(
    [Parameter(Mandatory=$true)][string]$List,
    [Parameter(Mandatory=$true)][string]$OutDir,
    [int]$SettleMs = 6000,
    [int]$WindowTimeoutMs = 20000,
    [string]$WorkDir = "C:\Work\pureikyubu\build",
    [string]$Exe = "C:\Work\pureikyubu\build\pureikyubu_sdl.exe",
    [int]$Skip = 0
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
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    public struct POINT { public int X; public int Y; }
}
"@

# Find the "Video Output" window that belongs to the emulator process we started.
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

# Does the bitmap hold anything but one flat colour?
function Test-BitmapFlat([System.Drawing.Bitmap]$bmp) {
    $w = $bmp.Width; $h = $bmp.Height
    $first = $bmp.GetPixel(0, 0).ToArgb()
    $sx = [Math]::Max(1, [int]($w / 32)); $sy = [Math]::Max(1, [int]($h / 32))
    for ($y = 0; $y -lt $h; $y += $sy) {
        for ($x = 0; $x -lt $w; $x += $sx) {
            if ($bmp.GetPixel($x, $y).ToArgb() -ne $first) { return $false }
        }
    }
    return $true
}

function Save-WindowShot([IntPtr]$hwnd, [string]$out) {
    if ([Win]::IsIconic($hwnd)) { [void][Win]::ShowWindow($hwnd, 9) }

    # PrintWindow renders the whole window (frame included), so the shot is taken at window size
    # and then cropped to the client area, which is the emulated picture and nothing else.
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
    # 2 = PW_RENDERFULLCONTENT: ask DWM for the composited window content.
    $ok = [Win]::PrintWindow($hwnd, $hdc, 2)
    $g.ReleaseHdc($hdc)
    $g.Dispose()

    $method = "printwindow"
    if (-not $ok -or (Test-BitmapFlat $full)) {
        # Fallback: put the window in front and read the screen rectangle it occupies.
        [void][Win]::ShowWindow($hwnd, 9)
        [void][Win]::BringWindowToTop($hwnd)
        [void][Win]::SetForegroundWindow($hwnd)
        Start-Sleep -Milliseconds 500
        $g2 = [System.Drawing.Graphics]::FromImage($full)
        $g2.CopyFromScreen($wr.Left, $wr.Top, 0, 0, (New-Object System.Drawing.Size $ww, $wh))
        $g2.Dispose()
        $method = "screen"
    }

    $client = New-Object System.Drawing.Bitmap $cw, $ch
    $gc = [System.Drawing.Graphics]::FromImage($client)
    $gc.DrawImage($full, (New-Object System.Drawing.Rectangle 0, 0, $cw, $ch),
                  (New-Object System.Drawing.Rectangle $ox, $oy, $cw, $ch),
                  [System.Drawing.GraphicsUnit]::Pixel)
    $gc.Dispose()
    $full.Dispose()

    $flat = Test-BitmapFlat $client
    $client.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
    $client.Dispose()
    if ($flat) { return "flat" }
    return $method
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$log = Join-Path $WorkDir "emu_log.txt"
$env:EMU_LOG = $log

$rows = @()
$demos = @(Get-Content $List | Where-Object { $_.Trim() -ne "" })
if ($Skip -gt 0) { $demos = $demos[$Skip..($demos.Count - 1)] }
$index = $Skip

foreach ($demo in $demos) {
    $index++
    $name = [System.IO.Path]::GetFileNameWithoutExtension($demo)
    $cat = Split-Path (Split-Path $demo -Parent) -Leaf
    $tag = "$cat" + "__" + "$name"
    Write-Host ("[{0}] === {1} ===" -f $index, $tag)

    if (Test-Path $log) { Remove-Item $log -Force }

    $proc = Start-Process -FilePath $Exe -ArgumentList @($demo) -WorkingDirectory $WorkDir -PassThru

    $hwnd = [IntPtr]::Zero
    $waited = 0
    while ($waited -lt $WindowTimeoutMs) {
        Start-Sleep -Milliseconds 400
        $waited += 400
        if ($proc.HasExited) { break }
        $hwnd = Find-VideoWindow $proc.Id
        if ($hwnd -ne [IntPtr]::Zero) { break }
    }

    $running = -not $proc.HasExited
    $shot = Join-Path $OutDir "$tag.png"
    $capture = "none"

    if ($hwnd -ne [IntPtr]::Zero -and $running) {
        Start-Sleep -Milliseconds $SettleMs
        $running = -not $proc.HasExited
        if ($running) { $capture = Save-WindowShot $hwnd $shot }
    }

    $exitCode = "?"
    if (-not $proc.HasExited) { $proc.Kill(); $proc.WaitForExit(); $exitCode = "killed" }
    else { $exitCode = $proc.ExitCode }

    $logDst = Join-Path $OutDir "$tag.log"
    if (Test-Path $log) { Copy-Item $log $logDst -Force }
    $logLines = 0
    if (Test-Path $logDst) { $logLines = (Get-Content $logDst | Measure-Object -Line).Lines }

    # The SDK's OSReport text (the emulated UART, channel Info) is what the text-only demos print;
    # keep it on its own so a demo can be read without digging through the hardware log.
    $reportDst = Join-Path $OutDir "$tag.txt"
    if (Test-Path $logDst) {
        $info = Select-String -Path $logDst -Pattern "^\[Info\] ?" | ForEach-Object { $_.Line -replace "^\[Info\] ?", "" }
        if ($info) { $info | Set-Content -Path $reportDst -Encoding UTF8 }
    }
    $reportLines = 0
    if (Test-Path $reportDst) { $reportLines = (Get-Content $reportDst | Measure-Object -Line).Lines }

    $rows += [pscustomobject]@{
        tag = $tag; demo = $demo
        window = ($hwnd -ne [IntPtr]::Zero)
        capture = $capture
        exit = $exitCode
        logLines = $logLines
        reportLines = $reportLines
    }
    Write-Host ("    window={0} capture={1} exit={2} logLines={3} reportLines={4}" -f ($hwnd -ne [IntPtr]::Zero), $capture, $exitCode, $logLines, $reportLines)
}

$csv = Join-Path $OutDir "sweep.csv"
if (Test-Path $csv) {
    $rows | Export-Csv -Path $csv -NoTypeInformation -Encoding UTF8 -Append
} else {
    $rows | Export-Csv -Path $csv -NoTypeInformation -Encoding UTF8
}
Write-Host "SWEEP DONE"
