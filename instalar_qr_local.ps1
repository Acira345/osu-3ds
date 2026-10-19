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

try {
    while ($true) {
        $client = $tcpListener.AcceptTcpClient()
        try {
            $client.SendBufferSize = 32768
            $client.SendTimeout = 20000
            $client.ReceiveTimeout = 10000
            $stream = $client.GetStream()
            $reader = New-Object System.IO.StreamReader($stream, [System.Text.Encoding]::ASCII, $false, 1024, $true)

            $requestLine = $reader.ReadLine()
            if (-not $requestLine) {
                continue
            }

            # Leer cabeceras HTTP (incluyendo posible Range de FBI)
            $rangeStart = 0
            $rangeEnd = -1
            $hasRange = $false
            while (($line = $reader.ReadLine()) -and $line.Trim().Length -gt 0) {
                if ($line -match '^Range:\s*bytes=(\d+)-(\d*)') {
                    $hasRange = $true
                    $rangeStart = [int64]$matches[1]
                    if ($matches[2] -ne "") {
                        $rangeEnd = [int64]$matches[2]
                    }
                }
            }

            $parts = $requestLine.Split(' ')
            $method = $parts[0]
            $path = if ($parts.Length -ge 2) { $parts[1] } else { "/" }

            Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Solicitud de la 3DS: $method $path" -ForegroundColor Cyan

            if ($path -eq "/osu-3ds.cia" -or $path -like "*/osu-3ds.cia*") {
                # Recargar siempre el .cia mas reciente del disco
                $fileBytes = [System.IO.File]::ReadAllBytes($ciaPath)
                $fileSize = $fileBytes.Length

                if ($rangeEnd -lt 0 -or $rangeEnd -ge $fileSize) {
                    $rangeEnd = $fileSize - 1
                }
                $contentLength = ($rangeEnd - $rangeStart) + 1

                if ($hasRange) {
                    $header = "HTTP/1.1 206 Partial Content`r`n" +
                              "Content-Type: application/octet-stream`r`n" +
                              "Content-Length: $contentLength`r`n" +
                              "Content-Range: bytes $rangeStart-$rangeEnd/$fileSize`r`n" +
                              "Accept-Ranges: bytes`r`n" +
                              "Connection: close`r`n`r`n"
                } else {
                    $header = "HTTP/1.1 200 OK`r`n" +
                              "Content-Type: application/octet-stream`r`n" +
                              "Content-Length: $fileSize`r`n" +
                              "Accept-Ranges: bytes`r`n" +
                              "Content-Disposition: attachment; filename=`"osu-3ds.cia`"`r`n" +
                              "Connection: close`r`n`r`n"
                }

                $headerBytes = [System.Text.Encoding]::ASCII.GetBytes($header)
                $stream.Write($headerBytes, 0, $headerBytes.Length)

                if ($method -ne "HEAD") {
                    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Enviando osu-3ds.cia ($([math]::Round($contentLength/1MB, 2)) MB) a la 3DS..." -ForegroundColor Yellow
                    # Bloques de 16 KB optimizados para el buffer Wi-Fi (socU) de la Nintendo 3DS
                    $chunkSize = 16384
                    $offset = [int]$rangeStart
                    $endOffset = [int]$rangeEnd + 1
                    $lastPct = -1

                    while ($offset -lt $endOffset) {
                        $toSend = [Math]::Min($chunkSize, $endOffset - $offset)
                        $stream.Write($fileBytes, $offset, $toSend)
                        $offset += $toSend

                        $pct = [int]((($offset - $rangeStart) * 100) / $contentLength)
                        if ($pct -ge ($lastPct + 25) -and $pct -lt 100) {
                            $lastPct = $pct
                            Write-Host "  -> Progreso: $pct% enviado..." -ForegroundColor DarkCyan
                        }
                    }
                    $stream.Flush()
                    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Envio 100% completado con exito a la consola!" -ForegroundColor Green
                }
            } else {
                $notFound = "HTTP/1.1 404 Not Found`r`nContent-Length: 0`r`nConnection: close`r`n`r`n"
                $nfBytes = [System.Text.Encoding]::ASCII.GetBytes($notFound)
                $stream.Write($nfBytes, 0, $nfBytes.Length)
                $stream.Flush()
            }
        } catch {
            Write-Host "[$(Get-Date -Format 'HH:mm:ss')] [Info] La 3DS cerro la conexion temporal (sondeo de FBI o reintento Wi-Fi). Servidor listo y esperando..." -ForegroundColor DarkYellow
        } finally {
            try { $client.Close() } catch {}
        }
    }
} finally {
    $tcpListener.Stop()
}
