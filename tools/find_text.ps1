$lines = Get-Content 'source/main.cpp' -Encoding UTF8
$calls = @('drawText', 'drawCenteredText', 'drawCenteredTextWithShadow', 'drawTextWithShadow', 'drawKeyBadge')
$ignore = @('osu!', 'osu!3DS', '3DS', 'A', 'B', 'X', 'Y', 'L', 'R', 'ZL', 'ZR', 'SELECT', 'START', '^ ', '< >', 'OK', '300', '100', '50', 'SS', 'S', 'D', 'F', 'C')

for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    if ($line.Trim().StartsWith('//')) { continue }
    foreach ($c in $calls) {
        if ($line.Contains($c)) {
            $regex = [regex]'\"([^\"\\]*(?:\\.[^\"\\]*)*)\"'
            $matches = $regex.Matches($line)
            foreach ($m in $matches) {
                $val = $m.Groups[1].Value
                if ($val.Length -gt 1 -and -not $val.StartsWith('%') -and -not $val.StartsWith('sdmc:') -and -not $val.StartsWith('romfs:') -and -not ($ignore -contains $val)) {
                    Write-Output "L$($i+1): $val"
                }
            }
        }
    }
}
