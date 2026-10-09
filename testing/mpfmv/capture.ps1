# Capture a guest frame profile of the Metroid Prime intro FMV.
#
#   .\capture.ps1                       # the defaults: the NTSC v1.02 image, the intro movie
#   .\capture.ps1 -MovieFrames 30       # a shorter capture
#   .\capture.ps1 -Iso 'D:\Isos\other.iso' -Movie '/Video/attract0.thp'
#
# The script resolves the disc range of the THP stream out of the image's own file system table
# (tools/isofst.py) and hands it to the emulator's `--movie` option, which is what lets the
# profiler mark the frames that read the stream. See ..\..\wiki\guestprof.md.

param(
    [string] $Iso = 'D:\Isos\Metroid Prime (v1.02)(USA).iso',

    # The stream to profile. `00_first_start.thp` is the intro FMV the title plays after the
    # texture logo screens - the one this research is about.
    [string] $Movie = '/Video/00_first_start.thp',

    [string] $Out = "$PSScriptRoot\capture.json",

    # How many frames of the movie the capture should hold, and the hard cap on all of them.
    [int] $MovieFrames = 60,
    [int] $GuestFrames = 6000,

    [string] $Emu = "$PSScriptRoot\..\..\scripts\VS2026\x64\Release\pureikyubu_headless.exe",

    [string] $Log = "$PSScriptRoot\capture.log"
)

$ErrorActionPreference = 'Stop'

$repo = Resolve-Path "$PSScriptRoot\..\.."
$iso = (Resolve-Path -LiteralPath $Iso).Path
$emu = (Resolve-Path -LiteralPath $Emu).Path

# The emulator reads its settings and its ROM images from `Data` next to the working directory
# (`build/Data`), so it has to be started there - from anywhere else it reports "Default settings
# missing" and the machine comes up on garbage.
$work = Join-Path $repo 'build'
if (-not (Test-Path (Join-Path $work 'Data\DefaultSettings.json'))) {
    throw "the emulator's data folder is missing: $work\Data (build the emulator first)"
}

Write-Host "image    : $iso"
Write-Host "emulator : $emu"
Write-Host "workdir  : $work"

# --- the disc range of the movie stream -------------------------------------------------------

$range = (& python "$PSScriptRoot\tools\isofst.py" $iso --range $Movie).Trim()
if ($LASTEXITCODE -ne 0 -or $range -notmatch '^0x[0-9A-Fa-f]+:\d+$') {
    throw "cannot resolve $Movie in the image (got '$range')"
}
Write-Host "movie    : $Movie at $range"

$info = & python "$PSScriptRoot\tools\isofst.py" $iso --thpinfo $Movie
$info | Where-Object { $_ -match '^\s+(disc offset|disc length|fps|frames|width|height|duration_s|audio)\s' } |
    ForEach-Object { Write-Host "           $_" }

# --- the capture ------------------------------------------------------------------------------

$env:EMU_LOG = $Log

Write-Host ""
Write-Host "capturing: --guestprof ... --movie $range --movieframes $MovieFrames --guestframes $GuestFrames"
Write-Host "the emulator boots the title first, so this takes a while (the capture is written when it stops)."
Write-Host ""

$sw = [Diagnostics.Stopwatch]::StartNew()

Push-Location $work
try {
    & $emu --guestprof $iso $Out --movie $range --movieframes $MovieFrames --guestframes $GuestFrames
    $status = $LASTEXITCODE
}
finally {
    Pop-Location
}

$sw.Stop()

Write-Host ""
Write-Host ("emulator finished in {0:hh\:mm\:ss} (status {1})" -f $sw.Elapsed, $status)
Write-Host "capture  : $Out ($((Get-Item -LiteralPath $Out).Length) bytes)"
Write-Host "log      : $Log"

Write-Host ""
Write-Host "next: python $PSScriptRoot\tools\mkreport.py $Out"

exit $status
