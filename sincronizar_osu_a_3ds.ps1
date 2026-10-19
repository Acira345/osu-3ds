param(
    [Parameter(Mandatory=$false)]
    [string]$DestinoSD = "",
    [Parameter(Mandatory=$false)]
    [string]$Ip3DS = ""
)

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
[System.Net.ServicePointManager]::Expect100Continue = $false
try {
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12 -bor [System.Net.SecurityProtocolType]::Tls11 -bor [System.Net.SecurityProtocolType]::Tls
} catch {}

Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "   osu! 3DS - Sincronizador Universal de Mapas (Stable + Lazer)" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan

$stablePath  = Join-Path $env:LOCALAPPDATA "osu!\Songs"
$lazerRoot   = Join-Path $env:APPDATA "osu"
$lazerExport = Join-Path $lazerRoot "exports"
$lazerFiles  = Join-Path $lazerRoot "files"
$hasFfmpeg   = [bool](Get-Command "ffmpeg" -ErrorAction SilentlyContinue)
$hasCurl     = [bool](Get-Command "curl.exe" -ErrorAction SilentlyContinue)

$hasStable      = Test-Path $stablePath
$hasLazerExport = Test-Path $lazerExport
$hasLazerFiles  = Test-Path $lazerFiles

Write-Host ""
Write-Host "Instalaciones detectadas en tu PC:" -ForegroundColor Yellow
Write-Host ("  - osu!stable (Songs)   : " + ($(if ($hasStable) { "DETECTADO ($stablePath)" } else { "No encontrado" }))) -ForegroundColor ($(if ($hasStable) { "Green" } else { "DarkGray" }))
Write-Host ("  - osu!lazer (Exports)  : " + ($(if ($hasLazerExport) { "DETECTADO ($lazerExport)" } else { "No encontrado" }))) -ForegroundColor ($(if ($hasLazerExport) { "Green" } else { "DarkGray" }))
Write-Host ("  - osu!lazer (Database) : " + ($(if ($hasLazerFiles) { "DETECTADO ($lazerFiles)" } else { "No encontrado" }))) -ForegroundColor ($(if ($hasLazerFiles) { "Green" } else { "DarkGray" }))
Write-Host ("  - Conversor Video 3DS  : " + ($(if ($hasFfmpeg) { "FFmpeg disponible (256x128 @ 15fps)" } else { "Sin FFmpeg (se usara el conversor web si envias por Wi-Fi)" }))) -ForegroundColor Cyan
Write-Host ""

if (-not $hasStable -and -not $hasLazerExport -and -not $hasLazerFiles) {
    $customPath = Read-Host "No se detecto la ruta por defecto. Escribe la ruta de tu carpeta de osu! (o Enter para salir)"
    if ([string]::IsNullOrWhiteSpace($customPath) -or -not (Test-Path $customPath)) { exit 0 }
    if (Test-Path (Join-Path $customPath "files")) {
        $lazerRoot = $customPath
        $lazerExport = Join-Path $lazerRoot "exports"
        $lazerFiles = Join-Path $lazerRoot "files"
        $hasLazerFiles = $true
        $hasLazerExport = Test-Path $lazerExport
    } else {
        $stablePath = $customPath
        $hasStable = $true
    }
}

# =========================================================================
# MENÚ 1: SELECCIONAR QUÉ VERSIÓN DE OSU! EXPORTAR
# =========================================================================
Write-Host "¿Que instalacion de osu! deseas sincronizar a tu 3DS?" -ForegroundColor Yellow
Write-Host "  [1] Solo osu!stable (Canciones de %localappdata%\osu!\Songs)"
Write-Host "  [2] Solo osu!lazer  (Exportaciones o biblioteca de %appdata%\osu)"
Write-Host "  [3] Ambas instalaciones (osu!stable y osu!lazer)"
$eleccionOsu = Read-Host "Selecciona 1, 2 o 3 (Por defecto: 1)"
if ([string]::IsNullOrWhiteSpace($eleccionOsu)) { $eleccionOsu = "1" }

$exportarStable = ($eleccionOsu -eq "1" -or $eleccionOsu -eq "3")
$exportarLazer  = ($eleccionOsu -eq "2" -or $eleccionOsu -eq "3")

$modoLazer = "3"
if ($exportarLazer) {
    Write-Host ""
    Write-Host "¿Como deseas obtener los mapas de osu!lazer?" -ForegroundColor Yellow
    Write-Host "  [1] Paquetes .osz exportados (Ctrl+E en osu!lazer -> exports)"
    Write-Host "  [2] Escanear biblioteca interna instalada (%appdata%\osu\files)"
    Write-Host "  [3] Ambos (primero exportados, luego biblioteca)"
    $modoLazer = Read-Host "Selecciona 1, 2 o 3 (Por defecto: 1)"
    if ([string]::IsNullOrWhiteSpace($modoLazer)) { $modoLazer = "1" }
}

# =========================================================================
# MENÚ 2: MÉTODO DE DESTINO (SD O WI-FI)
# =========================================================================
$modoWiFi = $false
if (-not [string]::IsNullOrWhiteSpace($Ip3DS)) {
    $modoWiFi = $true
} elseif ([string]::IsNullOrWhiteSpace($DestinoSD)) {
    Write-Host ""
    Write-Host "¿Como deseas pasar tus canciones a la Nintendo 3DS?" -ForegroundColor Yellow
    Write-Host "  [1] Copiar a la Tarjeta SD o carpeta local en PC (Mas rapido para muchos mapas)"
    Write-Host "  [2] Enviar por Wi-Fi directo a la 3DS (Abre 'DESCARGAR WI-FI' en la 3DS)"
    Write-Host "  [3] Iniciar Servidor Puente HTTP (Proxy Local para descargas en la 3DS)"
    $op = Read-Host "Selecciona 1, 2 o 3 (Por defecto: 1)"
    if ($op -eq "3") {
        Write-Host ""
        Write-Host "================================================================" -ForegroundColor Cyan
        Write-Host "   Iniciando Servidor Puente HTTP para Nintendo 3DS (Puerto 8081)" -ForegroundColor Cyan
        Write-Host "================================================================" -ForegroundColor Cyan
        $myIps = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -notmatch '^(127\.|169\.254\.)' }).IPAddress
        $primaryIp = if ($myIps) { $myIps[0] } else { "127.0.0.1" }
        Write-Host "Direccion del Puente: http://${primaryIp}:8081/" -ForegroundColor Green
        Write-Host "Este servicio permite a la 3DS descargar beatmaps de Cloudflare sin errores SSL." -ForegroundColor Gray
        Write-Host "Presiona Ctrl + C en esta ventana para detener el puente cuando termines." -ForegroundColor DarkGray
        Write-Host ""

        $listener = New-Object System.Net.HttpListener
        try {
            $listener.Prefixes.Add("http://+:8081/")
            $listener.Start()
        } catch {
            $listener = New-Object System.Net.HttpListener
            $listener.Prefixes.Add("http://localhost:8081/")
            $listener.Start()
        }
        Write-Host "✓ Puente activo y escuchando peticiones..." -ForegroundColor Green

        while ($listener.IsListening) {
            $ctx = $listener.GetContext()
            $req = $ctx.Request
            $res = $ctx.Response
            $rawUrl = $req.RawUrl
            Write-Host "[3DS Request] $($req.HttpMethod) $rawUrl" -ForegroundColor Cyan

            if ($rawUrl -match "^/d/(\d+)") {
                $setId = $matches[1]
                Write-Host "  Descargando BeatmapSet #$setId desde servidor espejo..." -ForegroundColor Yellow
                $oszUrl = "https://catboy.best/d/$setId"
                try {
                    $webClient = New-Object System.Net.WebClient
                    $webClient.Headers.Add("User-Agent", "Mozilla/5.0 (Nintendo 3DS)")
                    $data = $webClient.DownloadData($oszUrl)
                    $res.ContentType = "application/octet-stream"
                    $res.ContentLength64 = $data.Length
                    $res.OutputStream.Write($data, 0, $data.Length)
                    $res.OutputStream.Close()
                    Write-Host "  ✓ Enviado a la 3DS con exito ($([math]::Round($data.Length/1MB, 2)) MB)" -ForegroundColor Green
                } catch {
                    $res.StatusCode = 500
                    $errMsg = [System.Text.Encoding]::UTF8.GetBytes("Error al descargar set #$setId")
                    $res.OutputStream.Write($errMsg, 0, $errMsg.Length)
                    $res.OutputStream.Close()
                }
            } else {
                $res.StatusCode = 200
                $msg = [System.Text.Encoding]::UTF8.GetBytes("Puente HTTP de osu! 3DS Activo")
                $res.OutputStream.Write($msg, 0, $msg.Length)
                $res.OutputStream.Close()
            }
        }
        exit 0
    } elseif ($op -eq "2") {
        $modoWiFi = $true
        $Ip3DS = Read-Host "Escribe la IP que muestra tu 3DS (ej: 192.168.1.45 o http://192.168.1.45:8080)"
    } else {
        $DestinoSD = Read-Host "Escribe la letra de tu SD (ej: E:) o deja vacio para guardar en .\songs_para_3ds"
        if ([string]::IsNullOrWhiteSpace($DestinoSD)) {
            $DestinoSD = Join-Path $PSScriptRoot "songs_para_3ds"
        } elseif ($DestinoSD -match "^[a-zA-Z]:\\?$") {
            $DestinoSD = Join-Path ($DestinoSD.TrimEnd('\') + "\") "3ds\osu-3ds\songs"
        }
    }
}

if ($modoWiFi) {
    $Ip3DS = $Ip3DS.Trim()
    if (-not $Ip3DS.StartsWith("http://") -and -not $Ip3DS.StartsWith("https://")) {
        if ($Ip3DS -notmatch ":\d+$") { $Ip3DS = "${Ip3DS}:8080" }
        $Ip3DS = "http://$Ip3DS"
    }
    $Ip3DS = $Ip3DS.TrimEnd('/')
    Write-Host ""
    Write-Host "Probando conexion con la 3DS en: $Ip3DS ..." -ForegroundColor Cyan
    try {
        $testRes = Invoke-WebRequest -Uri $Ip3DS -UseBasicParsing -TimeoutSec 4 -ErrorAction Stop
        Write-Host "  ¡Conexion exitosa con tu Nintendo 3DS!" -ForegroundColor Green
    } catch {
        Write-Host "  [AVISO] No se recibio respuesta inmediata de $Ip3DS." -ForegroundColor Yellow
        Write-Host "  Verifica que:" -ForegroundColor Gray
        Write-Host "   1. Tu 3DS este encendida con la pantalla 'DESCARGAR WI-FI' abierta (pulsa SELECT en el menu)." -ForegroundColor Gray
        Write-Host "   2. Tu PC y la 3DS esten conectados a la misma red Wi-Fi." -ForegroundColor Gray
        Write-Host "   3. La IP sea exactamente la que aparece en la pantalla superior de la 3DS." -ForegroundColor Gray
        $cont = Read-Host "¿Deseas intentar enviar de todos modos? (S/N, por defecto S)"
        if ($cont -and $cont -match "^[nN]$") { exit 0 }
    }
} else {
    New-Item -ItemType Directory -Force -Path $DestinoSD | Out-Null
    Write-Host "Carpeta de destino: $DestinoSD" -ForegroundColor Green
}

function Send-OszTo3DS([string]$filePath, [string]$fileName) {
    $uri = "$Ip3DS/upload?name=$([uri]::EscapeDataString($fileName))"
    
    # 1. Probar primero con curl.exe si está disponible (rápido, nativo y sin problemas de cabeceras Expect en Windows)
    if ($hasCurl) {
        try {
            $output = & curl.exe -s -S -w "%{http_code}" -X POST -H "Content-Type: application/octet-stream" --data-binary "@$filePath" "$uri" 2>&1
            if ($output -match "200|OK") {
                Write-Host "  [Wi-Fi OK] Enviado e instalado en 3DS: $fileName" -ForegroundColor Green
                return $true
            }
        } catch {}
    }

    # 2. Fallback con Invoke-RestMethod con Expect100Continue = false
    try {
        Invoke-RestMethod -Uri $uri -Method Post -InFile $filePath -ContentType "application/octet-stream" -TimeoutSec 120 -ErrorAction Stop | Out-Null
        Write-Host "  [Wi-Fi OK] Enviado e instalado en 3DS: $fileName" -ForegroundColor Green
        return $true
    } catch {
        $errDetail = $_.Exception.Message
        Write-Host "  [Error Wi-Fi] No se pudo enviar $fileName a $Ip3DS ($errDetail)" -ForegroundColor Red
        return $false
    }
}

$totalProcesados = 0

# =========================================================================
# 1. IMPORTAR DESDE OSU!STABLE (%localappdata%\osu!\Songs)
# =========================================================================
if ($exportarStable -and $hasStable) {
    $stableFolders = Get-ChildItem -Path $stablePath -Directory -ErrorAction SilentlyContinue
    if ($stableFolders.Count -gt 0) {
        Write-Host ""
        Write-Host "Se encontraron $($stableFolders.Count) carpetas de canciones en osu!stable." -ForegroundColor Cyan
        $filtro = Read-Host "Escribe parte del nombre para filtrar, un numero maximo (ej: 25), o pulsa Enter para TODAS (0 = todas)"
        if ($filtro -and $filtro -ne "0" -and $filtro -match "^\d+$") {
            $stableFolders = $stableFolders | Select-Object -First ([int]$filtro)
        } elseif (-not [string]::IsNullOrWhiteSpace($filtro) -and $filtro -ne "0") {
            $stableFolders = $stableFolders | Where-Object { $_.Name -like "*$filtro*" }
        }

        Write-Host "Procesando $($stableFolders.Count) carpetas de osu!stable..." -ForegroundColor Yellow
        foreach ($dir in $stableFolders) {
            $osuCheck = Get-ChildItem -Path $dir.FullName -Filter "*.osu" -File -ErrorAction SilentlyContinue
            if (-not $osuCheck) { continue }

            if ($modoWiFi) {
                $tmpZip = Join-Path $env:TEMP ($dir.Name + ".osz")
                if (Test-Path $tmpZip) { Remove-Item $tmpZip -Force }
                $validFiles = Get-ChildItem -Path $dir.FullName -File | Where-Object {
                    $_.Name -notmatch "@2x\." -and $_.Extension -match "^\.(osu|ini|mp3|wav|ogg|jpg|jpeg|png|3dsv|mjpg)$"
                }
                if ($validFiles) {
                    Compress-Archive -Path $validFiles.FullName -DestinationPath $tmpZip -Force
                    Send-OszTo3DS -filePath $tmpZip -fileName ($dir.Name + ".osz") | Out-Null
                    Remove-Item $tmpZip -Force -ErrorAction SilentlyContinue
                    $totalProcesados++
                }
            } else {
                $targetSub = Join-Path $DestinoSD $dir.Name
                New-Item -ItemType Directory -Force -Path $targetSub | Out-Null
                $filesToCopy = Get-ChildItem -Path $dir.FullName -File | Where-Object {
                    $_.Name -notmatch "@2x\." -and $_.Extension -match "^\.(osu|ini|mp3|wav|ogg|jpg|jpeg|png|3dsv|mjpg)$"
                }
                foreach ($f in $filesToCopy) {
                    Copy-Item $f.FullName -Destination (Join-Path $targetSub $f.Name) -Force
                }
                if ($hasFfmpeg) {
                    $vid = Get-ChildItem -Path $dir.FullName -Include *.mp4, *.avi, *.webm, *.mkv -File -ErrorAction SilentlyContinue | Select-Object -First 1
                    if ($vid) {
                        $outMjpg = Join-Path $targetSub "video.mjpg"
                        if (-not (Test-Path $outMjpg)) {
                            Write-Host "  [Video 3DS] Optimizando $($vid.Name) a 256x128 @ 15fps..." -ForegroundColor Yellow
                            & ffmpeg -y -i $vid.FullName -vf "scale=256:128" -r 15 -an -c:v mjpeg -q:v 8 -f mjpeg $outMjpg 2>$null
                        }
                    }
                }
                Write-Host "  [SD OK] Copiado: $($dir.Name)" -ForegroundColor Green
                $totalProcesados++
            }
        }
    }
}

# =========================================================================
# 2. IMPORTAR EXPORTACIONES DIRECTAS DE OSU!LAZER (%appdata%\osu\exports)
# =========================================================================
if ($exportarLazer -and ($modoLazer -eq "1" -or $modoLazer -eq "3") -and $hasLazerExport) {
    $exportedOsz = Get-ChildItem -Path $lazerExport -Filter "*.osz" -File -ErrorAction SilentlyContinue
    if ($exportedOsz.Count -gt 0) {
        Write-Host ""
        Write-Host "Se encontraron $($exportedOsz.Count) paquetes .osz en osu!lazer\exports." -ForegroundColor Cyan
        foreach ($osz in $exportedOsz) {
            if ($modoWiFi) {
                Send-OszTo3DS -filePath $osz.FullName -fileName $osz.Name | Out-Null
            } else {
                Copy-Item $osz.FullName -Destination (Join-Path $DestinoSD $osz.Name) -Force
                Write-Host "  [SD OK] Copiado paquete .osz: $($osz.Name)" -ForegroundColor Green
            }
            $totalProcesados++
        }
    } else {
        Write-Host ""
        Write-Host "No se encontraron archivos .osz en $lazerExport." -ForegroundColor DarkGray
        Write-Host "Tip: En osu!lazer puedes presionar Ctrl+E en cualquier mapa para exportarlo a esta carpeta." -ForegroundColor Gray
    }
}

# =========================================================================
# 3. ESCANEAR BIBLIOTECA INTERNA DE OSU!LAZER (%appdata%\osu\files)
# =========================================================================
if ($exportarLazer -and ($modoLazer -eq "2" -or $modoLazer -eq "3") -and $hasLazerFiles) {
    Write-Host ""
    Write-Host "Escaneando biblioteca interna de osu!lazer ($lazerFiles)..." -ForegroundColor Cyan
    $lazerMapSets = @{}
    $allHashFiles = Get-ChildItem -Path $lazerFiles -File -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.Length -gt 120 -and $_.Length -lt 1500000 }

    foreach ($hf in $allHashFiles) {
        try {
            $fs = [System.IO.File]::OpenRead($hf.FullName)
            $buf = New-Object byte[] 28
            $n = $fs.Read($buf, 0, 28)
            $fs.Close()
            if ($n -lt 15) { continue }
            $hdr = [System.Text.Encoding]::UTF8.GetString($buf, 0, $n)
            if ($hdr -notmatch "osu file format") { continue }

            $lines = Get-Content $hf.FullName -TotalCount 60 -Encoding UTF8 -ErrorAction SilentlyContinue
            $setId = ""
            $title = ""
            $artist = ""
            foreach ($ln in $lines) {
                if ($ln -match "^BeatmapSetID\s*:\s*(\d+)") { $setId = $Matches[1].Trim() }
                elseif ($ln -match "^Title\s*:\s*(.+)")      { $title = $Matches[1].Trim() }
                elseif ($ln -match "^Artist\s*:\s*(.+)")     { $artist = $Matches[1].Trim() }
            }
            if (-not [string]::IsNullOrWhiteSpace($setId) -and -not $lazerMapSets.ContainsKey($setId)) {
                $lazerMapSets[$setId] = @{
                    SetId  = $setId
                    Title  = $(if ($title) { $title } else { "Beatmap $setId" })
                    Artist = $(if ($artist) { $artist } else { "osu!lazer" })
                }
            }
        } catch {}
    }

    if ($lazerMapSets.Count -gt 0) {
        Write-Host "¡Se detectaron $($lazerMapSets.Count) canciones unicas en tu biblioteca de osu!lazer!" -ForegroundColor Green
        $setsList = @($lazerMapSets.Values)
        for ($i = 0; $i -lt [Math]::Min(15, $setsList.Count); $i++) {
            Write-Host ("  [{0}] #{1} - {2} - {3}" -f ($i + 1), $setsList[$i].SetId, $setsList[$i].Artist, $setsList[$i].Title) -ForegroundColor Gray
        }
        if ($setsList.Count -gt 15) {
            Write-Host "  ... y $($setsList.Count - 15) canciones mas." -ForegroundColor DarkGray
        }

        $resp = Read-Host "¿Deseas descargar e instalar estas canciones de tu biblioteca de osu!lazer para la 3DS? (S/N o escribe texto para filtrar)"
        if ($resp -and $resp -notmatch "^[nN]$") {
            if ($resp -notmatch "^[sSyY]$") {
                $setsList = $setsList | Where-Object { $_.Title -like "*$resp*" -or $_.Artist -like "*$resp*" }
            }

            foreach ($s in $setsList) {
                $safeName = ("$($s.SetId) $($s.Artist) - $($s.Title)").Split([IO.Path]::GetInvalidFileNameChars()) -join "_"
                $oszFileName = "$safeName.osz"
                $destOsz = $(if ($modoWiFi) { Join-Path $env:TEMP $oszFileName } else { Join-Path $DestinoSD $oszFileName })

                Write-Host "Obteniendo '$($s.Artist) - $($s.Title)' (Set #$($s.SetId))..." -ForegroundColor Yellow
                $mirrors = @(
                    "https://catboy.best/d/$($s.SetId)n",
                    "https://api.nerinyan.moe/d/$($s.SetId)?noVideo=1",
                    "https://osu.direct/api/d/$($s.SetId)?noVideo=1"
                )
                $downloaded = $false
                foreach ($mUrl in $mirrors) {
                    try {
                        Invoke-WebRequest -Uri $mUrl -OutFile $destOsz -UseBasicParsing -TimeoutSec 60
                        if ((Test-Path $destOsz) -and (Get-Item $destOsz).Length -gt 1024) {
                            $downloaded = $true
                            break
                        }
                    } catch {}
                }

                if ($downloaded) {
                    if ($modoWiFi) {
                        Send-OszTo3DS -filePath $destOsz -fileName $oszFileName | Out-Null
                        Remove-Item $destOsz -Force -ErrorAction SilentlyContinue
                    } else {
                        Write-Host "  [SD OK] Guardado: $oszFileName" -ForegroundColor Green
                    }
                    $totalProcesados++
                } else {
                    Write-Host "  [!] No se pudo descargar el Set #$($s.SetId) de los espejos." -ForegroundColor Red
                }
            }
        }
    }
}

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "  Proceso finalizado. Total de mapas transferidos: $totalProcesados" -ForegroundColor Green
if (-not $modoWiFi -and $totalProcesados -gt 0) {
    Write-Host "  Nota: Al iniciar osu-3ds en tu consola, los archivos .osz en" -ForegroundColor Gray
    Write-Host "  'sdmc:/3ds/osu-3ds/songs/' se extraeran e instalaran solos." -ForegroundColor Gray
}
Write-Host "================================================================" -ForegroundColor Cyan
