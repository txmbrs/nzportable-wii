param([string]$TargetRoot)
$ErrorActionPreference = 'Stop'
try {
    if (-not $TargetRoot) {
        $candidates = @(Get-PSDrive -PSProvider FileSystem | Where-Object {
            Test-Path -LiteralPath (Join-Path $_.Root 'apps\nzportable\nzp') -PathType Container
        })
        if ($candidates.Count -eq 1) { $TargetRoot = $candidates[0].Root }
        else { $TargetRoot = Read-Host 'Podaj dysk karty SD / USB (np. E:\)' }
    }
    $app = [IO.Path]::GetFullPath((Join-Path $TargetRoot 'apps\nzportable'))
    if (-not (Test-Path -LiteralPath (Join-Path $app 'nzp') -PathType Container)) {
        throw 'Nie znaleziono apps/nzportable/nzp. Wybierz dysk z obecna gra.'
    }
    $manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'cooperation-r18.json') -Raw | ConvertFrom-Json
    Write-Host 'Sprawdzam obecne dane gry. Nie beda kopiowane ponownie...'
    foreach ($file in $manifest.shared) {
        $path = Join-Path (Join-Path $app 'nzp') $file.path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Brak danych gry: $($file.path)" }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256) {
            throw "Inna wersja danych gry: $($file.path). Aktualizacja wymaga danych paczki r17."
        }
    }
    foreach ($file in $manifest.files) {
        $source = Join-Path (Join-Path $PSScriptRoot 'apps\nzportable') $file.path
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $file.sha256) {
            throw "Uszkodzony plik paczki: $($file.path)"
        }
    }
    $boot = Join-Path $app 'boot.dol'
    $backup = Join-Path $app 'boot-before-cooperation.dol'
    if ((Test-Path -LiteralPath $boot) -and -not (Test-Path -LiteralPath $backup)) {
        Copy-Item -LiteralPath $boot -Destination $backup
    }
    foreach ($file in $manifest.files) {
        $source = Join-Path (Join-Path $PSScriptRoot 'apps\nzportable') $file.path
        $destination = [IO.Path]::GetFullPath((Join-Path $app $file.path))
        if (-not $destination.StartsWith($app + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid package path.' }
        if ($file.path -eq 'online/wii-online.cfg' -and (Test-Path -LiteralPath $destination)) { continue }
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination -Force
        if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $file.sha256) {
            throw "Blad kopiowania: $($file.path)"
        }
    }
    $seedPath = Join-Path $app 'online\tls-seed.bin'
    if (-not (Test-Path -LiteralPath $seedPath)) {
        $seed = New-Object byte[] 64
        $random = [Security.Cryptography.RandomNumberGenerator]::Create()
        try { $random.GetBytes($seed) } finally { $random.Dispose() }
        $stream = [IO.File]::Open($seedPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
        try { $stream.Write($seed,0,$seed.Length); $stream.Flush($true) } finally { $stream.Dispose(); [Array]::Clear($seed,0,$seed.Length) }
    } elseif ((Get-Item -LiteralPath $seedPath).Length -ne 64) {
        throw 'Istniejacy tls-seed.bin ma zly rozmiar. Zachowano go bez zmian.'
    }
    Write-Host "Gotowe: $app" -ForegroundColor Green
    Write-Host 'Uruchom NZPortable i wybierz COOPERATIVE. Online wymaga sprawdzenia na Wii.'
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
