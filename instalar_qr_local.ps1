# =============================================================================
# Servidor Local TCP para Instalacion por QR en FBI (Nintendo 3DS)
# Sin requerir permisos de administrador (usa sockets TcpListener directos)
# =============================================================================

$port = 8090
$ciaPath = Join-Path $PSScriptRoot "osu-3ds.cia"

if (-not (Test-Path $ciaPath)) {
    Write-Host "Error: No se encontro osu-3ds.cia en $PSScriptRoot" -ForegroundColor Red
    pause
    exit 1
}

# Obtener IP activa de Wi-Fi / LAN
$ipObj = Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
    $_.InterfaceAlias -match 'Wi-Fi|Ethernet' -and $_.IPAddress -notmatch '^169\.254\.'
} | Select-Object -First 1

$ip = if ($ipObj) { $ipObj.IPAddress } else { "127.0.0.1" }
$ciaUrl = "http://${ip}:${port}/osu-3ds.cia"
$qrApiUrl = "https://api.qrserver.com/v1/create-qr-code/?size=320x320&margin=12&data=" + [uri]::EscapeDataString($ciaUrl)

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   INSTALADOR QR DIRECTO PARA FBI (NINTENDO 3DS)" -ForegroundColor Magenta
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "IP de tu PC : $ip" -ForegroundColor White
Write-Host "URL del CIA : $ciaUrl" -ForegroundColor Green
Write-Host "En tu 3DS   : Abre FBI -> Remote Install -> Scan QR Code" -ForegroundColor Yellow
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Abriendo imagen con el Codigo QR en tu navegador..." -ForegroundColor Gray
Write-Host "Presiona Ctrl+C para cerrar el servidor cuando termine." -ForegroundColor DarkGray
Write-Host ""

Start-Process $qrApiUrl

# Iniciar servidor socket TCP puro (no requiere permisos de administrador de http.sys)
$endpoint = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, $port)
$tcpListener = New-Object System.Net.Sockets.TcpListener($endpoint)

try {
    $tcpListener.Start()
} catch {
    Write-Host "Error al iniciar en puerto $port`: $_" -ForegroundColor Red
    pause
    exit 1
}

$fileBytes = [System.IO.File]::ReadAllBytes($ciaPath)
$fileSize = $fileBytes.Length

try {
    while ($true) {
        $client = $tcpListener.AcceptTcpClient()
        $stream = $client.GetStream()
        $reader = New-Object System.IO.StreamReader($stream)
        
        $requestLine = $reader.ReadLine()
        if (-not $requestLine) {
            $client.Close()
            continue
        }

        # Leer resto de cabeceras HTTP
        while (($line = $reader.ReadLine()) -and $line.Trim().Length -gt 0) {}

        $method = $requestLine.Split(' ')[0]
        $path = $requestLine.Split(' ')[1]

        Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Solicitud de la 3DS: $method $path" -ForegroundColor Cyan

        if ($path -eq "/osu-3ds.cia" -or $path -like "*/osu-3ds.cia*") {
            $header = "HTTP/1.1 200 OK`r`n" +
                      "Content-Type: application/octet-stream`r`n" +
                      "Content-Length: $fileSize`r`n" +
                      "Content-Disposition: attachment; filename=`"osu-3ds.cia`"`r`n" +
                      "Connection: close`r`n`r`n"

            $headerBytes = [System.Text.Encoding]::ASCII.GetBytes($header)
            $stream.Write($headerBytes, 0, $headerBytes.Length)

            if ($method -ne "HEAD") {
                Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Enviando osu-3ds.cia ($([math]::Round($fileSize/1MB, 2)) MB) a la 3DS..." -ForegroundColor Yellow
                $chunkSize = 65536
                $offset = 0
                while ($offset -lt $fileSize) {
                    $toSend = [Math]::Min($chunkSize, $fileSize - $offset)
                    $stream.Write($fileBytes, $offset, $toSend)
                    $offset += $toSend
                }
                Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Envio completado con exito a la consola!" -ForegroundColor Green
            }
        } else {
            $notFound = "HTTP/1.1 404 Not Found`r`nContent-Length: 0`r`nConnection: close`r`n`r`n"
            $nfBytes = [System.Text.Encoding]::ASCII.GetBytes($notFound)
            $stream.Write($nfBytes, 0, $nfBytes.Length)
        }

        $stream.Flush()
        $client.Close()
    }
} finally {
    $tcpListener.Stop()
}
