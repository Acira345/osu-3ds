param(
    [Parameter(Mandatory=$false)]
    [string]$CarpetaCanciones = ".\songs"
)

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host "  osu! 3DS - Conversor de Videos de Fondo (256x128 15fps)" -ForegroundColor Cyan
Write-Host "=========================================================" -ForegroundColor Cyan

if (-not (Get-Command "ffmpeg" -ErrorAction SilentlyContinue)) {
    Write-Host "[!] No se encontro 'ffmpeg' en el PATH de Windows." -ForegroundColor Yellow
    Write-Host "    Nota: Si instalas canciones por el Gestor Wi-Fi de la 3DS," -ForegroundColor Gray
    Write-Host "    tu navegador web convierte el video a .3dsv automaticamente" -ForegroundColor Gray
    Write-Host "    sin necesitar ffmpeg instalado." -ForegroundColor Gray
    exit 1
}

if (-not (Test-Path $CarpetaCanciones)) {
    Write-Host "[!] La carpeta '$CarpetaCanciones' no existe." -ForegroundColor Red
    Write-Host "    Uso: .\convertir_videos_3ds.ps1 -CarpetaCanciones 'E:\3ds\osu-3ds\songs'" -ForegroundColor Gray
    exit 1
}

$videos = Get-ChildItem -Path $CarpetaCanciones -Recurse -Include *.mp4, *.avi, *.webm, *.mkv, *.flv
if ($videos.Count -eq 0) {
    Write-Host "No se encontraron videos (.mp4/.avi/.webm/.mkv) en '$CarpetaCanciones'." -ForegroundColor Yellow
    exit 0
}

foreach ($vid in $videos) {
    $outMjpg = Join-Path $vid.DirectoryName ($vid.BaseName + ".mjpg")
    Write-Host "Convirtiendo: $($vid.Name) -> $(Split-Path $outMjpg -Leaf) ..." -ForegroundColor Green
    & ffmpeg -y -i $vid.FullName -vf "scale=256:128" -r 15 -an -c:v mjpeg -q:v 8 -f mjpeg $outMjpg 2>$null
    if (Test-Path $outMjpg) {
        $origMB = [math]::Round($vid.Length / 1MB, 2)
        $newMB  = [math]::Round((Get-Item $outMjpg).Length / 1MB, 2)
        Write-Host "  [OK] Reducido de $origMB MB a $newMB MB" -ForegroundColor Cyan
    }
}

Write-Host "¡Conversion completada!" -ForegroundColor Green
