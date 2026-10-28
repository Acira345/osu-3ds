param(
    [Parameter(Mandatory=$false)]
    [string]$songfolder = ".\songs"
)

Write-Host "=============================================================" -ForegroundColor Cyan
Write-Host "  osu! 3DS - Converter for Bachground Videos (256x128 15fps)" -ForegroundColor Cyan
Write-Host "=============================================================" -ForegroundColor Cyan

if (-not (Get-Command "ffmpeg" -ErrorAction SilentlyContinue)) {
    Write-Host "[!] Can't find 'ffmpeg' in the Windows PATH." -ForegroundColor Yellow
    Write-Host "    Note: If you install songs via the 3DS Wifi Manager," -ForegroundColor Gray
    Write-Host "    your web browser will automatically convert the video to .3dsv," -ForegroundColor Gray
    Write-Host "    without requiring ffmpeg to be installed." -ForegroundColor Gray
    exit 1
}

if (-not (Test-Path $songfolder)) {
    Write-Host "[!] The folder '$songfolder' does not exist." -ForegroundColor Red
    Write-Host "    Use: .\convert_videos_3ds.ps1 -songfolder 'E:\3ds\osu-3ds\songs'" -ForegroundColor Gray
    exit 1
}

$videos = Get-ChildItem -Path $songfolder -Recurse -Include *.mp4, *.avi, *.webm, *.mkv, *.flv
if ($videos.Count -eq 0) {
    Write-Host "No videos (.mp4/.avi/.webm/.mkv) were found in '$songfolder'." -ForegroundColor Yellow
    exit 0
}

foreach ($vid in $videos) {
    $outMjpg = Join-Path $vid.DirectoryName ($vid.BaseName + ".mjpg")
    Write-Host "Converting: $($vid.Name) -> $(Split-Path $outMjpg -Leaf) ..." -ForegroundColor Green
    & ffmpeg -y -i $vid.FullName -vf "scale=256:128" -r 15 -an -c:v mjpeg -q:v 8 -f mjpeg $outMjpg 2>$null
    if (Test-Path $outMjpg) {
        $origMB = [math]::Round($vid.Length / 1MB, 2)
        $newMB  = [math]::Round((Get-Item $outMjpg).Length / 1MB, 2)
        Write-Host "  [OK] Reduced $origMB MB to $newMB MB" -ForegroundColor Cyan
    }
}

Write-Host "Conversion complete!" -ForegroundColor Green
