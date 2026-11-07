[CmdletBinding()]
param(
    [string]$Url,
    [string]$Sha256,
    [string]$Destination = (Join-Path ([Environment]::GetFolderPath('UserProfile')) 'Downloads'),
    [switch]$Legacy
)

# AURELIA image downloader/stager.
# It never invents a release URL: it either uses a local ISO that exists,
# downloads a URL supplied by the caller, or copies the legacy demo with -Legacy.
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$Root = $PSScriptRoot
$CurrentIso = Join-Path $Root 'distro\live-image-amd64.hybrid.iso'
$LegacyIso = Join-Path $Root 'build\aurelia.iso'

if ($Url -and $Legacy) {
    throw 'Use either -Url or -Legacy, not both.'
}

$download = $false
$source = $null
$label = $null

if ($Url) {
    $uri = $null
    if (-not [Uri]::TryCreate($Url, [UriKind]::Absolute, [ref]$uri) -or
        $uri.Scheme -notin @('http', 'https')) {
        throw 'The -Url value must be an absolute http or https URL.'
    }
    $download = $true
    $source = $Url
    $label = 'published AURELIA ISO'
}
elseif ($Legacy) {
    if (-not (Test-Path -LiteralPath $LegacyIso)) {
        throw "The legacy image does not exist at $LegacyIso."
    }
    $source = $LegacyIso
    $label = 'legacy AURELIA kernel demo'
    Write-Warning 'The legacy image is the 5.5 MB custom-kernel demo, not the Ubuntu/KDE release.'
}
elseif (Test-Path -LiteralPath $CurrentIso) {
    $source = $CurrentIso
    $label = 'current AURELIA Ubuntu/KDE ISO'
}
else {
    Write-Host 'No current AURELIA Ubuntu/KDE ISO was found.'
    Write-Host ''
    Write-Host "Expected local build: $CurrentIso"
    Write-Host 'Build it on Ubuntu 26.04 with:'
    Write-Host '  cd distro'
    Write-Host '  sudo scripts/build-iso.sh'
    Write-Host ''
    Write-Host 'Or download a published image:'
    Write-Host '  .\download-aurelia.cmd -Url <published-iso-url> -Sha256 <sha256>'
    Write-Host ''
    Write-Host 'The only local bootable image is the legacy 5.5 MB kernel demo.'
    Write-Host 'Use -Legacy to copy it explicitly.'
    exit 1
}

if (-not (Test-Path -LiteralPath $Destination)) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
}
if (-not (Test-Path -LiteralPath $Destination -PathType Container)) {
    throw "Destination is not a directory: $Destination"
}

$name = if ($Url) { [IO.Path]::GetFileName($uri.AbsolutePath) } else { Split-Path -Leaf $source }
$name = [IO.Path]::GetFileName($name)
if ([string]::IsNullOrWhiteSpace($name)) {
    $name = 'aurelia.iso'
}
$output = Join-Path $Destination $name

Write-Host "AURELIA source: $label"
Write-Host "Destination:   $output"

if ($download) {
    $part = "$output.part"
    try {
        Write-Host "Downloading $Url"
        Invoke-WebRequest -Uri $uri -OutFile $part -UseBasicParsing
        Move-Item -LiteralPath $part -Destination $output -Force
    }
    finally {
        if (Test-Path -LiteralPath $part) {
            Remove-Item -LiteralPath $part -Force
        }
    }
}
else {
    $sourceFull = [IO.Path]::GetFullPath($source)
    $outputFull = [IO.Path]::GetFullPath($output)
    if ($sourceFull -ne $outputFull) {
        Write-Host "Copying local image..."
        Copy-Item -LiteralPath $source -Destination $output -Force
    }
    else {
        Write-Host 'Image is already at the destination.'
    }
}

$actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $output).Hash.ToLowerInvariant()
$expected = $Sha256

if (-not $expected -and -not $download) {
    $sidecar = "$source.sha256"
    if (Test-Path -LiteralPath $sidecar) {
        $line = Get-Content -LiteralPath $sidecar -TotalCount 1
        $candidate = ($line -split '\s+')[0]
        if ($candidate -match '^[0-9a-fA-F]{64}$') {
            $expected = $candidate
        }
    }
}

if ($expected) {
    $expected = $expected.Trim().ToLowerInvariant()
    if ($actual -ne $expected) {
        Remove-Item -LiteralPath $output -Force
        throw "SHA-256 mismatch. Expected $expected but got $actual. The file was removed."
    }
    Write-Host 'SHA-256 verified.'
}
else {
    Write-Host "SHA-256: $actual"
    Write-Host 'No expected checksum was supplied, so integrity was not independently verified.'
}

Write-Host "Saved: $output"
exit 0
