# Start a demo with the MCP server, press pad keys with SendInput, then run a JDI command and
# print its answer.  Used to read the TEV registers for a state that only the pad can reach.
param(
    [string]$Demo = "C:\DolphinSDK\HW2\bin\demos\gxdemo\tev-swap.elf",
    [string]$Exe = "C:\Work\pureikyubu\build\pureikyubu_sdl.exe",
    [string]$WorkDir = "C:\Work\pureikyubu\build",
    [int]$SettleMs = 8000,
    [int]$KeyWaitMs = 1500,
    [string]$Cmd = "gxregs",
    [string]$CmdArg = "tev",
    # semicolon/comma-separated pad key names, sent in order (e.g. "A;A;B")
    [string]$Keys = ""
)

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class MK {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern uint SendInput(uint n, INPUT[] inp, int size);
  [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk; public ushort wScan; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
  [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx; public int dy; public uint mouseData; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
  [StructLayout(LayoutKind.Sequential)] public struct HARDWAREINPUT { public uint uMsg; public ushort wParamL; public ushort wParamH; }
  [StructLayout(LayoutKind.Explicit)] public struct INPUTUNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; [FieldOffset(0)] public HARDWAREINPUT hi; }
  [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public INPUTUNION u; }
  public static void Key(ushort scan, bool up) {
    INPUT[] a = new INPUT[1];
    a[0].type = 1;
    a[0].u.ki.wVk = 0; a[0].u.ki.wScan = scan;
    a[0].u.ki.dwFlags = (uint)(0x0008 | (up ? 0x0002 : 0));
    a[0].u.ki.time = 0; a[0].u.ki.dwExtraInfo = IntPtr.Zero;
    SendInput(1, a, Marshal.SizeOf(typeof(INPUT)));
  }
}
"@

$KEYMAP = @{
    "X" = 0x1F; "Y" = 0x1E; "R" = 0x11; "A" = 0x2D; "B" = 0x2C; "START" = 0x1C
    "UP" = 0x48; "DOWN" = 0x50; "LEFT" = 0x4B; "RIGHT" = 0x4D
    "DPAD_UP" = 0x47; "DPAD_DOWN" = 0x4F; "DPAD_LEFT" = 0x53; "DPAD_RIGHT" = 0x51
}

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $Exe
$psi.Arguments = "--mcp `"$Demo`""
$psi.WorkingDirectory = $WorkDir
$psi.RedirectStandardInput = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.EnvironmentVariables["EMU_LOG"] = (Join-Path $WorkDir "emu_log_mcp.txt")

$p = [System.Diagnostics.Process]::Start($psi)
Write-Host "started $($p.Id)"

function Send([string]$json) { $p.StandardInput.WriteLine($json); $p.StandardInput.Flush() }

Send '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"probe","version":"1"}}}'
Write-Host "initialize: $($p.StandardOutput.ReadLine() | Out-String -Width 200 | ForEach-Object { $_.Substring(0, [Math]::Min(80, $_.Length)) })"

Start-Sleep -Milliseconds $SettleMs

$keyList = $Keys -split "[,; ]+" | Where-Object { $_ -ne "" }
foreach ($k in $keyList) {
    if (-not $KEYMAP.ContainsKey($k)) { Write-Host "unknown key $k"; continue }
    [void][MK]::SetForegroundWindow($p.MainWindowHandle)
    Start-Sleep -Milliseconds 300
    [MK]::Key([uint16]$KEYMAP[$k], $false)
    Start-Sleep -Milliseconds 180
    [MK]::Key([uint16]$KEYMAP[$k], $true)
    Write-Host "pressed $k"
    Start-Sleep -Milliseconds $KeyWaitMs
}

$req = '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"' + $Cmd + '","arguments":{"args":["' + $CmdArg + '"]}}}'
Send $req
$resp = $p.StandardOutput.ReadLine()
$outFile = Join-Path (Split-Path $WorkDir -Parent) "tevoneop\mcp_resp.json"
if (-not (Test-Path (Split-Path $outFile -Parent))) { $outFile = "C:\Work\tevoneop\mcp_resp.json" }
Set-Content -Path $outFile -Value $resp -Encoding UTF8
Write-Host "REGS written to $outFile ($($resp.Length) chars)"

if (-not $p.HasExited) { $p.Kill() }
Write-Host "DONE"
