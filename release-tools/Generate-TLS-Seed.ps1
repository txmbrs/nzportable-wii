param([string]$OutputDirectory = (Join-Path $PSScriptRoot 'apps/nzportable/online'))
$ErrorActionPreference = 'Stop'
$seedPath = Join-Path ([System.IO.Path]::GetFullPath($OutputDirectory)) 'tls-seed.bin'
if ([System.IO.File]::Exists($seedPath)) {
    if ((Get-Item -LiteralPath $seedPath).Length -ne 64) {
        throw 'Existing seed is not 64 bytes. It was preserved; verify your installation.'
    }
    Write-Host 'Existing private seed preserved. No change made.'
    exit 0
}
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($seedPath)) | Out-Null
$seedBytes = New-Object byte[] 64
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
$stream = $null
try {
    $rng.GetBytes($seedBytes)
    $stream = [System.IO.File]::Open($seedPath, [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    $stream.Write($seedBytes, 0, $seedBytes.Length)
    $stream.Flush($true)
} finally {
    if ($null -ne $stream) { $stream.Dispose() }
    $rng.Dispose()
    [Array]::Clear($seedBytes, 0, $seedBytes.Length)
}
Write-Host 'Private TLS seed created. Upload apps/nzportable/online/tls-seed.bin over FTP.'
Write-Host 'Create a separate seed for each Wii. Keep the existing seed when updating.'
