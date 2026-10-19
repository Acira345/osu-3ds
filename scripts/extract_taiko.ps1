Add-Type -AssemblyName System.Drawing

function IsBgColor([System.Drawing.Color]$c) {
    if ($c.B -gt 160 -and $c.G -gt 150 -and $c.R -gt 140) {
        $diff1 = [Math]::Abs($c.R - $c.G)
        $diff2 = [Math]::Abs($c.G - $c.B)
        if ($diff1 -le 20 -and $diff2 -le 20) {
            return $true
        }
    }
    return $false
}

function ExtractSprite {
    param(
        [System.Drawing.Bitmap]$srcImg,
        [int]$x, [int]$y, [int]$w, [int]$h,
        [int]$dstW, [int]$dstH,
        [string]$outPath
    )

    $crop = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gCrop = [System.Drawing.Graphics]::FromImage($crop)
    $srcRect = New-Object System.Drawing.Rectangle($x, $y, $w, $h)
    $dstRect = New-Object System.Drawing.Rectangle(0, 0, $w, $h)
    $gCrop.DrawImage($srcImg, $dstRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
    $gCrop.Dispose()

    # Flood fill outer background to transparent
    $visited = New-Object 'bool[,]' $w, $h
    $queue = New-Object System.Collections.Generic.Queue[System.Drawing.Point]

    for ($i = 0; $i -lt $w; $i++) {
        $queue.Enqueue((New-Object System.Drawing.Point($i, 0)))
        $queue.Enqueue((New-Object System.Drawing.Point($i, ($h - 1))))
        $visited[$i, 0] = $true
        $visited[$i, ($h - 1)] = $true
    }
    for ($j = 0; $j -lt $h; $j++) {
        $queue.Enqueue((New-Object System.Drawing.Point(0, $j)))
        $queue.Enqueue((New-Object System.Drawing.Point(($w - 1), $j)))
        $visited[0, $j] = $true
        $visited[($w - 1), $j] = $true
    }

    $transparent = [System.Drawing.Color]::FromArgb(0, 0, 0, 0)

    while ($queue.Count -gt 0) {
        $pt = $queue.Dequeue()
        $px = $pt.X
        $py = $pt.Y
        $c = $crop.GetPixel($px, $py)

        if (IsBgColor $c) {
            $crop.SetPixel($px, $py, $transparent)

            $dirs = @( @(1,0), @(-1,0), @(0,1), @(0,-1) )
            foreach ($d in $dirs) {
                $nx = $px + $d[0]
                $ny = $py + $d[1]
                if ($nx -ge 0 -and $nx -lt $w -and $ny -ge 0 -and $ny -lt $h) {
                    if (-not $visited[$nx, $ny]) {
                        $visited[$nx, $ny] = $true
                        $queue.Enqueue((New-Object System.Drawing.Point($nx, $ny)))
                    }
                }
            }
        }
    }

    # Scale down to low-res for 3DS
    $scaled = New-Object System.Drawing.Bitmap($dstW, $dstH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gScaled = [System.Drawing.Graphics]::FromImage($scaled)
    $gScaled.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $gScaled.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $gScaled.DrawImage($crop, (New-Object System.Drawing.Rectangle(0, 0, $dstW, $dstH)))
    $gScaled.Dispose()
    $crop.Dispose()

    $scaled.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $scaled.Dispose()
    Write-Output "Saved: $outPath ($dstW x $dstH)"
}

$assetPath = "C:\Users\Diyel\.gemini\antigravity\brain\ebf14675-09f1-4a1f-b4f3-3d9f33e3add3\.user_uploaded\media_1792380533214_cab3210d.jpg"
$img = [System.Drawing.Bitmap]::FromFile($assetPath)

# 1. Drums
ExtractSprite $img 72 40 248 212 180 154 "romfs/gfx/taiko/drum_idle.png"
ExtractSprite $img 388 40 250 212 180 154 "romfs/gfx/taiko/drum_don.png"
ExtractSprite $img 682 40 290 212 210 154 "romfs/gfx/taiko/drum_kat.png"

# 2. Notes (Normal 36x36 & Big 52x52)
ExtractSprite $img 37 266 118 102 36 36 "romfs/gfx/taiko/don_note.png"
ExtractSprite $img 512 266 112 102 36 36 "romfs/gfx/taiko/kat_note.png"
ExtractSprite $img 37 266 118 102 52 52 "romfs/gfx/taiko/don_big.png"
ExtractSprite $img 512 266 112 102 52 52 "romfs/gfx/taiko/kat_big.png"

# 3. Hit Effects
ExtractSprite $img 520 634 138 94 48 36 "romfs/gfx/taiko/hit_burst.png"
ExtractSprite $img 390 526 138 112 48 40 "romfs/gfx/taiko/hit_star.png"

# 4. Mascots
ExtractSprite $img 36 532 242 208 64 56 "romfs/gfx/taiko/don_stand.png"
ExtractSprite $img 660 536 164 194 48 56 "romfs/gfx/taiko/don_dancer.png"
ExtractSprite $img 825 536 174 194 50 56 "romfs/gfx/taiko/kat_dancer.png"

$img.Dispose()
Write-Output "Extraction complete!"
