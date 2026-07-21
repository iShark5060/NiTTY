#!/usr/bin/env pwsh
# NiTTY quality gate: configure and Release-build with CMake (Windows).
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    if (-not (Test-Path LATEST.VER)) {
        throw 'LATEST.VER is missing'
    }

    $generators = @(
        'Visual Studio 18 2026',
        'Visual Studio 17 2022'
    )
    $configured = $false
    foreach ($gen in $generators) {
        Write-Host "==> cmake -S . -B build -G `"$gen`" -A x64"
        if (Test-Path build) {
            Remove-Item -Recurse -Force build
        }
        cmake -S . -B build -G $gen -A x64
        if ($LASTEXITCODE -eq 0) {
            $configured = $true
            break
        }
    }
    if (-not $configured) {
        throw 'No supported Visual Studio installation found (tried VS 2026 and VS 2022)'
    }

    Write-Host '==> cmake --build build --config Release --parallel'
    cmake --build build --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw 'cmake build failed' }

    Write-Host '==> validate passed'
}
finally {
    Pop-Location
}
