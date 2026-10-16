# =============================================================================
# Servidor Local de Instalacion por QR para FBI (Nintendo 3DS)
# Sirve osu-3ds.cia en tu red Wi-Fi local y abre una ventana con el Codigo QR
# para escanearlo directamente desde FBI -> Remote Install -> Scan QR Code
# =============================================================================

$port = 8090
$ciaPath = Join-Path $PSScriptRoot "osu-3ds.cia"

if (-not (Test-Path $ciaPath)) {
    Write-Host "Error: No se encontro osu-3ds.cia en $PSScriptRoot" -ForegroundColor Red
    exit 1
}

# Detectar la IP local Wi-Fi/LAN de la PC
$ip = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
    $_.InterfaceAlias -notmatch 'Loopback|vEthernet|WSL|VirtualBox|VMware' -and
    $_.IPAddress -notmatch '^169\.254\.'
} | Select-Object -First 1).IPAddress

if (-not $ip) { $ip = "127.0.0.1" }

$ciaUrl = "http://${ip}:${port}/osu-3ds.cia"
$qrApiUrl = "https://api.qrserver.com/v1/create-qr-code/?size=320x320&margin=12&data=" + [uri]::EscapeDataString($ciaUrl)

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  INSTALADOR QR DIRECTO PARA FBI (NINTENDO 3DS)" -ForegroundColor Magenta
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "URL del CIA : $ciaUrl" -ForegroundColor Green
Write-Host "En tu 3DS   : Abre FBI -> Remote Install -> Scan QR Code" -ForegroundColor Yellow
Write-Host "Presiona Ctrl+C en esta ventana cuando termine de instalar." -ForegroundColor Gray
Write-Host ""

# Abrir el QR en el navegador de la PC para escanearlo con la camara de la 3DS
Start-Process $qrApiUrl

# Iniciar servidor HTTP en el puerto 8090
$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://*:${port}/")
try {
    $listener.Start()
} catch {
    $listener = New-Object System.Net.HttpListener
    $listener.Prefixes.Add("http://${ip}:${port}/")
    $listener.Start()
}

try {
    while ($listener.IsListening) {
        $context = $listener.GetContext()
        $req = $context.Request
        $res = $context.Response
        Write-Host "[$(Get-Date -Format 'HH:mm:ss')] 3DS conectada: $($req.HttpMethod) $($req.Url.AbsolutePath)" -ForegroundColor Cyan

        if ($req.Url.AbsolutePath -eq "/osu-3ds.cia") {
            $bytes = [System.IO.File]::ReadAllBytes($ciaPath)
            $res.ContentType = "application/octet-stream"
            $res.ContentLength64 = $bytes.Length
            $res.AddHeader("Content-Disposition", "attachment; filename=`"osu-3ds.cia`"")
            if ($req.HttpMethod -ne "HEAD") {
                $res.OutputStream.Write($bytes, 0, $bytes.Length)
            }
            $res.Close()
            Write-Host "[$(Get-Date -Format 'HH:mm:ss')] osu-3ds.cia enviado con exito a tu 3DS!" -ForegroundColor Green
        } else {
            $res.StatusCode = 404
            $res.Close()
        }
    }
} finally {
    $listener.Stop()
}
