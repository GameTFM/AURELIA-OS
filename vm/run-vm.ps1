<#
.SYNOPSIS
Boot the AURELIA live ISO in a local QEMU virtual machine.

.DESCRIPTION
Verifies the ISO checksum when a .sha256 file sits beside it, picks an
accelerator that exists, and starts the same BIOS/virtio-vga configuration that
has been tested. Use -DiskGB to attach an empty disk for installer testing.
#>
[CmdletBinding()]
param(
    [string]$Iso = (Join-Path $PSScriptRoot "..\build\ci\artifact\aurelia-1.0-solstice.iso"),
    [string]$Qemu = "",
    [int]$MemoryMB = 4096,
    [int]$Cpus = 2,
    [ValidateSet("auto", "whpx", "tcg")][string]$Accel = "auto",
    [ValidateSet("gtk", "sdl")][string]$Display = "gtk",
    [int]$DiskGB = 0
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    Write-Error $Message
    exit 1
}

if (-not (Test-Path -LiteralPath $Iso)) {
    Fail "ISO not found: $Iso (pass -Iso <path>)"
}
$Iso = (Resolve-Path -LiteralPath $Iso).Path

$shaFile = "$Iso.sha256"
if (Test-Path -LiteralPath $shaFile) {
    $expected = ((Get-Content -LiteralPath $shaFile -TotalCount 1) -split '\s+')[0]
    $actual = (Get-FileHash -LiteralPath $Iso -Algorithm SHA256).Hash
    if ($expected -ne $actual) {
        Fail "SHA-256 mismatch: expected $expected, got $actual"
    }
    Write-Host "SHA-256 verified: $actual"
}

if (-not $Qemu) {
    $found = Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue
    if ($found) {
        $Qemu = $found.Source
    } else {
        $bundled = Join-Path $PSScriptRoot "..\toolchain\qemu\qemu-system-x86_64.exe"
        if (Test-Path -LiteralPath $bundled) { $Qemu = $bundled }
    }
}
if (-not $Qemu -or -not (Test-Path -LiteralPath $Qemu)) {
    Fail "qemu-system-x86_64 not found; install QEMU or pass -Qemu <path>"
}
$Qemu = (Resolve-Path -LiteralPath $Qemu).Path

if ($Accel -eq "auto") {
    $available = (& $Qemu -accel help 2>&1) -join "`n"
    $Accel = if ($available -match "whpx") { "whpx" } else { "tcg" }
}

$accelArgs = @("-accel", $Accel)
if ($Accel -eq "tcg") {
    $accelArgs += @("-cpu", "max")
    Write-Host "note: using TCG emulation (slower boot)"
} else {
    Write-Host "note: using WHPX; if it fails, run with -Accel tcg"
}

$diskArgs = @()
if ($DiskGB -gt 0) {
    $disk = Join-Path $PSScriptRoot "..\build\ci\aurelia-test.qcow2"
    if (-not (Test-Path -LiteralPath $disk)) {
        $img = Join-Path $PSScriptRoot "..\toolchain\qemu\qemu-img.exe"
        if (-not (Test-Path -LiteralPath $img)) {
            $cmd = Get-Command qemu-img -ErrorAction SilentlyContinue
            if ($cmd) { $img = $cmd.Source } else { Fail "qemu-img not found; cannot create the test disk" }
        }
        & $img create -f qcow2 $disk "$($DiskGB)G"
        Write-Host "created empty test disk: $disk ($DiskGB GB)"
    }
    $diskArgs = @("-drive", "file=$disk,if=virtio,format=qcow2")
}

$arguments = @(
    "-name", "AURELIA-VM",
    "-machine", "pc",
    "-m", "$MemoryMB",
    "-smp", "$Cpus"
) + $accelArgs + @(
    "-cdrom", $Iso,
    "-boot", "d",
    "-device", "virtio-vga",
    "-display", $Display,
    "-netdev", "user,id=net0",
    "-device", "e1000,netdev=net0",
    "-usb"
) + $diskArgs

Write-Host "Starting AURELIA: $Qemu"
& $Qemu @arguments
exit $LASTEXITCODE
