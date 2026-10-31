# ThermoBench build + test script
# Run from project root: .\build.ps1
# No PATH setup needed — this script finds the tools itself.

$mingw = "C:\msys64\mingw64\bin"

if (-not (Test-Path "$mingw\cmake.exe")) {
    Write-Host "ERROR: MSYS2 MinGW tools not found at $mingw" -ForegroundColor Red
    Write-Host "Install MSYS2 from https://www.msys2.org and run:" -ForegroundColor Yellow
    Write-Host "  pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja" -ForegroundColor Yellow
    exit 1
}

$env:PATH = "$mingw;" + $env:PATH

Write-Host ""
Write-Host "=== ThermoBench Build ===" -ForegroundColor Cyan
Write-Host "Compiler : $(& "$mingw\g++.exe" --version | Select-Object -First 1)"
Write-Host "CMake    : $(& "$mingw\cmake.exe" --version | Select-Object -First 1)"
Write-Host ""

# Configure (only if build/ doesn't have a cache already)
if (-not (Test-Path "build\CMakeCache.txt")) {
    Write-Host "Configuring..." -ForegroundColor Yellow
    & "$mingw\cmake.exe" -S . -B build -G Ninja
    if ($LASTEXITCODE -ne 0) { Write-Host "Configure FAILED" -ForegroundColor Red; exit 1 }
}

# Build
Write-Host "Building..." -ForegroundColor Yellow
& "$mingw\cmake.exe" --build build
if ($LASTEXITCODE -ne 0) { Write-Host "Build FAILED" -ForegroundColor Red; exit 1 }

Write-Host ""
Write-Host "=== Running Tests ===" -ForegroundColor Cyan
& ".\build\bin\RunTests.exe"
if ($LASTEXITCODE -ne 0) { Write-Host "Tests FAILED" -ForegroundColor Red; exit 1 }

Write-Host ""
Write-Host "All done! Start the server with:" -ForegroundColor Green
Write-Host "  .\start.ps1" -ForegroundColor White
